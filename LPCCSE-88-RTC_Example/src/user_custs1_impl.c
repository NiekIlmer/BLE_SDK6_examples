/**
 ****************************************************************************************
 *
 * @file user_custs1_impl.c
 *
 * @brief User implementation for custom profile.
 *
 * Copyright (C) 2012-2020 Dialog Semiconductor.
 * This computer program includes Confidential, Proprietary Information
 * of Dialog Semiconductor. All Rights Reserved.
 *
 ****************************************************************************************
 */

/**
 ****************************************************************************************
 * @addtogroup APP
 * @{
 ****************************************************************************************
 */

#if BLE_CUSTOM1_SERVER

/*
 * INCLUDE FILES
 ****************************************************************************************
 */
#include "user_custs1_impl.h"
#include "arch_console.h"
#include "user_custs1_def.h"
#include "custs1_task.h"
#include "rtc.h"
#include "app_task.h"
#include "user_real_time_clk.h"
#include "user_rtc_util.h"

timer_hnd ntf_update_tmr_hnd                            __SECTION_ZERO("retention_mem_area0"); //@RETENTION MEMORY
bool rec_alarm_flag                                     __SECTION_ZERO("retention_mem_area0"); //@RETENTION MEMORY

 /*
 * FUNCTION DEFINITIONS
 ****************************************************************************************
*/



bool is_alarm_recursive(void)
{
    return rec_alarm_flag;
}

static void rtc_error_ntf_send(uint16_t handle, uint8_t err_code)
{
    uint8_t error_code[5] = "ERR ";
    struct custs1_val_ntf_ind_req *req = KE_MSG_ALLOC_DYN(CUSTS1_VAL_NTF_REQ,
                                                      prf_get_task_from_id(TASK_ID_CUSTS1),
                                                      TASK_APP,
                                                      custs1_val_ntf_ind_req,
                                                      sizeof(error_code));
    
    error_code[4] = err_code;
    req->handle = handle;
    req->length = sizeof(error_code);
    req->notification = true;
    memcpy(req->value, error_code, sizeof(error_code));

    ke_msg_send(req);
}

void user_svc1_current_time_wr_ind_handler(ke_msg_id_t const msgid,
                                            struct custs1_val_write_ind const *param,
                                            ke_task_id_t const dest_id,
                                            ke_task_id_t const src_id)
{    
    rtc_time_t rtc_current_time;
    rtc_calendar_t rtc_current_date;
    
    struct current_time_char_structure current_timestamp;
    memset(&current_timestamp, 0, sizeof(struct current_time_char_structure));
    memcpy(&current_timestamp, &param->value[0], param->length);
    
    rtc_current_date.wday       = current_timestamp.wday;
    rtc_current_date.year       = current_timestamp.year;
    rtc_current_date.month      = current_timestamp.month;
    rtc_current_date.mday       = current_timestamp.mday;
    
    rtc_current_time.hour       = current_timestamp.hour;
    rtc_current_time.minute     = current_timestamp.minute;
    rtc_current_time.sec        = current_timestamp.second;
    rtc_current_time.hsec       = current_timestamp.hsecond;
    rtc_current_time.hour_mode  = current_timestamp.hour_mode;
    
    if (current_timestamp.hour_mode == RTC_HOUR_MODE_12H)
        rtc_current_time.pm_flag    = current_timestamp.pm_flag;

    // First have the RTC driver to check if the date/time entry is valid
    rtc_status_code_t status = rtc_set_time_clndr(&rtc_current_time, &rtc_current_date);
    
    if(status != RTC_STATUS_CODE_VALID_ENTRY)
    {
        rtc_error_ntf_send(param->handle, status);
    }
}

static void app_ntf_rtc_update_handler(void)
{
    struct current_time_char_structure updated_timestamp;
    rtc_time_t rtc_current_time;
    rtc_calendar_t rtc_current_date;    
    rtc_get_time_clndr(&rtc_current_time, &rtc_current_date);
    
    updated_timestamp.wday      = rtc_current_date.wday;
    updated_timestamp.year      = rtc_current_date.year;
    updated_timestamp.month     = rtc_current_date.month;
    updated_timestamp.mday      = rtc_current_date.mday;
    updated_timestamp.hour      = rtc_current_time.hour;
    updated_timestamp.minute    = rtc_current_time.minute;
    updated_timestamp.second    = rtc_current_time.sec;
    updated_timestamp.hsecond   = rtc_current_time.hsec;
    updated_timestamp.hour_mode = rtc_current_time.hour_mode;
    updated_timestamp.pm_flag   = rtc_current_time.pm_flag;
    
    struct custs1_val_ntf_ind_req *req = KE_MSG_ALLOC_DYN(CUSTS1_VAL_NTF_REQ,
                                                      prf_get_task_from_id(TASK_ID_CUSTS1),
                                                      TASK_APP,
                                                      custs1_val_ntf_ind_req,
                                                      DEF_SVC1_CURRENT_TIME_CHAR_LEN);
    
    req->handle = SVC1_IDX_CURRENT_TIME_VAL;
    req->length = DEF_SVC1_CURRENT_TIME_CHAR_LEN;
    req->notification = true;
    memcpy(req->value, &updated_timestamp, DEF_SVC1_CURRENT_TIME_CHAR_LEN);

    ke_msg_send(req);
    
    if (ke_state_get(TASK_APP) == APP_CONNECTED)
    {
        // Set it once again until Stop command is received in Control Characteristic
        ntf_update_tmr_hnd = app_easy_timer(APP_NTF_RTC_UPDATE, app_ntf_rtc_update_handler);
    }
}

void user_svc1_current_time_cfg_ind_handler(ke_msg_id_t const msgid,
                                            struct custs1_val_write_ind const *param,
                                            ke_task_id_t const dest_id,
                                            ke_task_id_t const src_id)
{
    uint16_t ntf_en = 0;
    memcpy (&ntf_en, param->value, param->length);
    if (ntf_en == PRF_CLI_START_NTF)
    {
        ntf_update_tmr_hnd = app_easy_timer(APP_NTF_RTC_UPDATE, app_ntf_rtc_update_handler);
    }
    else
    {
        if (ntf_update_tmr_hnd != EASY_TIMER_INVALID_TIMER)
        {
            app_easy_timer_cancel(ntf_update_tmr_hnd);
            ntf_update_tmr_hnd = EASY_TIMER_INVALID_TIMER;
        }
    }
}

/**
 ****************************************************************************************
 * @brief Performs validity checks for the alarm set from the peer 
 * @param[in] alarm_char_structure holds the peer alarm set data
 * @return rtc_status_code_t invalid or valid settings
 ****************************************************************************************
 */
static rtc_status_code_t user_check_alarm_set(struct alarm_char_structure *alarm_time)
{
    if(alarm_time->month != ALARM_IGNORE_DATE_VALUE && alarm_time->month > 12)
        return RTC_STATUS_CODE_INVALID_CLNDR_ALM;
    if(alarm_time->mday != ALARM_IGNORE_DATE_VALUE && alarm_time->mday > 31)
        return RTC_STATUS_CODE_INVALID_CLNDR_ALM;
    if(alarm_time->hour != ALARM_IGNORE_TIME_VALUE)
    {
        if (rtc_get_hour_clk_mode() == RTC_HOUR_MODE_12H && alarm_time->hour > 11)
            return RTC_STATUS_CODE_INVALID_TIME_ALM;
        if (alarm_time->hour > 23)
            return RTC_STATUS_CODE_INVALID_TIME_ALM;
    }
    if(alarm_time->minute != ALARM_IGNORE_TIME_VALUE && alarm_time->minute > 59)
        return RTC_STATUS_CODE_INVALID_TIME_ALM;
    if(alarm_time->second != ALARM_IGNORE_TIME_VALUE && alarm_time->second > 59)
        return RTC_STATUS_CODE_INVALID_TIME_ALM;
    if(alarm_time->hsecond != ALARM_IGNORE_TIME_VALUE && alarm_time->hsecond > 99)
        return RTC_STATUS_CODE_INVALID_TIME_ALM;
    if(alarm_time->pm_flag != 0 && rtc_get_hour_clk_mode() != RTC_HOUR_MODE_12H)
        return RTC_STATUS_CODE_INVALID_TIME_HOUR_MODE_ALM;
    
    return RTC_STATUS_CODE_VALID_ENTRY;
}

void user_svc1_alarm_wr_ind_handler(ke_msg_id_t const msgid,
                                    struct custs1_val_write_ind const *param,
                                    ke_task_id_t const dest_id,
                                    ke_task_id_t const src_id)
{
    uint8_t temp = 0x00;
    rtc_status_code_t status;
    
    rtc_time_t time;
    rtc_alarm_calendar_t calendar;
    memset(&time, 0x00, sizeof(rtc_time_t));
    memset(&calendar, 0x01, sizeof(rtc_alarm_calendar_t));
    rtc_alarm_calendar_t *p_calendar = &calendar;
    struct alarm_char_structure alarm_time;
    
    memset(&alarm_time, 0, sizeof(struct alarm_char_structure));
    memcpy(&alarm_time, &param->value[0], sizeof(struct alarm_char_structure));
    /* Validate the alert value passed from the application */
    status = user_check_alarm_set(&alarm_time);
    
    if (status == RTC_STATUS_CODE_VALID_ENTRY)
    {
        if(alarm_time.month != ALARM_IGNORE_DATE_VALUE)
        {
            calendar.month = alarm_time.month;
            temp |= RTC_ALARM_EN_MONTH;
        }
        
        if(alarm_time.mday != ALARM_IGNORE_DATE_VALUE)
        {
            calendar.mday = alarm_time.mday;
            temp |= RTC_ALARM_EN_MDAY;
        }
        
        if(!(temp & (RTC_ALARM_EN_MDAY|RTC_ALARM_EN_MONTH)))
            p_calendar = NULL;
            
        
        if(alarm_time.hour != ALARM_IGNORE_TIME_VALUE)
        {
            time.hour = alarm_time.hour;
            temp |= RTC_ALARM_EN_HOUR;
        }
        
        if(alarm_time.minute != ALARM_IGNORE_TIME_VALUE)
        {
            time.minute = alarm_time.minute;
            temp |= RTC_ALARM_EN_MIN;
        }
        
        if(alarm_time.second != ALARM_IGNORE_TIME_VALUE)
        {
            time.sec = alarm_time.second;
            temp |= RTC_ALARM_EN_SEC;
        }
        
        if(alarm_time.hsecond != ALARM_IGNORE_TIME_VALUE)
        {
            time.hsec = alarm_time.hsecond;
            temp |= RTC_ALARM_EN_HSEC;
        }
        
        time.hour_mode  = rtc_get_hour_clk_mode();
        
        if (time.hour_mode == RTC_HOUR_MODE_12H)
            time.pm_flag    = alarm_time.pm_flag;
        
        status = rtc_set_alarm(&time, p_calendar, temp);
    }
    
    if (status != RTC_STATUS_CODE_VALID_ENTRY)
    {
        rtc_error_ntf_send(param->handle, status);
#ifdef PRINT_DATE_TIME_DATA
        arch_printf("Invalid Alarm time, RTC return status %02x \n\r", status);
#endif
    }
    else
    {
        user_rtc_register_intr(rtc_wakeup_event, RTC_INTR_ALRM);        // Enable the alarm interrupt after we verify that the alarm is properly configured
        rec_alarm_flag = (alarm_time.recursive_alarm) ? true : false ;
#ifdef PRINT_DATE_TIME_DATA
        arch_printf("%s alarm was set for %02d/%02d %02d:%02d:%02d:%02d \n\r", ((rec_alarm_flag) ? "Recursive" : "One Time") ,alarm_time.mday, 
                                                                                alarm_time.month, time.hour, time.minute, time.sec, time.hsec);
#endif
    }
}

void user_svc1_current_time_read_ind_handler(ke_msg_id_t const msgid,
                                                struct custs1_value_req_ind const *param,
                                                ke_task_id_t const dest_id,
                                                ke_task_id_t const src_id)
{
    struct current_time_char_structure updated_timestamp;
    rtc_time_t rtc_current_time;
    rtc_calendar_t rtc_current_date;    
    rtc_get_time_clndr(&rtc_current_time, &rtc_current_date);
    
    updated_timestamp.wday      = rtc_current_date.wday;
    updated_timestamp.year      = rtc_current_date.year;
    updated_timestamp.month     = rtc_current_date.month;
    updated_timestamp.mday      = rtc_current_date.mday;
    updated_timestamp.hour      = rtc_current_time.hour;
    updated_timestamp.minute    = rtc_current_time.minute;
    updated_timestamp.second    = rtc_current_time.sec;
    updated_timestamp.hsecond   = rtc_current_time.hsec;
    updated_timestamp.hour_mode = rtc_current_time.hour_mode;
    updated_timestamp.pm_flag   = rtc_current_time.pm_flag;
   
    
    struct custs1_value_req_rsp *rsp = KE_MSG_ALLOC_DYN(CUSTS1_VALUE_REQ_RSP,
                                               prf_get_task_from_id(TASK_ID_CUSTS1),
                                               TASK_APP,
                                               custs1_value_req_rsp,
                                               DEF_SVC1_CURRENT_TIME_CHAR_LEN);
    
    // Provide the connection index.
    rsp->conidx  = app_env[param->conidx].conidx;
    // Provide the attribute index.
    rsp->att_idx = param->att_idx;
    // Force current length to zero.
    rsp->length  = sizeof(struct current_time_char_structure);
    // Provide the ATT error code.
    rsp->status  = ATT_ERR_NO_ERROR;
    // Copy value
    memcpy(&rsp->value, &updated_timestamp, rsp->length);
    // Send message
    ke_msg_send(rsp);
}

#endif // BLE_CUSTOM1_SERVER
