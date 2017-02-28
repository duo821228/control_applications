#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include <rtdm/rtdm.h>
#include <native/task.h>
#include <native/sem.h>
#include <native/mutex.h>
#include <native/timer.h>
#include <rtdk.h>
#include <pthread.h>

#include "ecrt.h"

#define SM_FRAME_PERIOD_NS      4000000 // 4ms.
#define YAS_ALIAS				0,0 // FIXME: This macro should be declared per slave.
#define YASKAWA					0x00000539, 0x02200301 // VendorId, ProductID

/* EtherCAT Configuration pointer */
static ec_master_t *master = NULL;

/* FIXME:  Declares per slave device. */
static ec_slave_config_t *slave0 = NULL;
/* FIXME: Process Data Offset in the domain. 
   Declares per slave device. */
unsigned int slave0_6040_00;
unsigned int slave0_607a_00;
unsigned int slave0_6041_00;
unsigned int slave0_6064_00;

/* FIXME: Declares per slave device. */
ec_pdo_entry_info_t slave_0_pdo_entries[] = {
	/* RxPDOs */
	{0x6040, 0x00, 16}, /*ControlWord*/
	{0x607a, 0x00, 32}, /*Target Position*/
	/* TxPDOs */
	{0x6041, 0x00, 16}, /*StatusWord*/
	{0x6064, 0x00, 32}, /*Position Actual*/
};

/* PDO maps for CSP (default) mode */ 
ec_pdo_info_t slave_0_pdos[] = {
	{0x1601, 2, slave_0_pdo_entries + 0}, /* 2nd Receive PDO mapping */
	{0x1a01, 2, slave_0_pdo_entries + 2}, /* 2nd Transit PDO mapping */
};

/* Sync manager configurations, FIXME: declares per slave device. */
ec_sync_info_t slave_0_syncs[] = {
	{0, EC_DIR_OUTPUT, 0, NULL, EC_WD_DISABLE},
	{1, EC_DIR_INPUT, 0, NULL, EC_WD_DISABLE},
	{2, EC_DIR_OUTPUT, 1, slave_0_pdos + 0, EC_WD_DISABLE},
	{3, EC_DIR_INPUT, 1, slave_0_pdos + 1, EC_WD_DISABLE},
	{0xff}
};

/* Domains, yaskawa drive requires separated domain. */
static ec_domain_t *domainRd = NULL;
static ec_domain_t *domainWrt = NULL;
uint8_t *domainRd_pd = NULL;
uint8_t *domainWrt_pd = NULL;

/* FIXME: Insert pdos after 1st slave code,
   if you want to add more slave devices.*/
const static ec_pdo_entry_reg_t domainRd_regs[] = {
	/* RxPDOs */
	/* ALIAS, Device infomation(Vid/Pid), PDO idx, PDO subidx, offsets */
	{YAS_ALIAS, YASKAWA, 0x6040, 0, &slave0_6040_00},
	{YAS_ALIAS, YASKAWA, 0x607a, 0, &slave0_607a_00},
	// Add here!
	{}
};
const static ec_pdo_entry_reg_t domainWrt_regs[] = {
	/* TxPDOs */
	{YAS_ALIAS, YASKAWA, 0x6041, 0, &slave0_6041_00},
	{YAS_ALIAS, YASKAWA, 0x6064, 0, &slave0_6064_00},
	// Add here!
	{}
};

/* Distributed Clock */
#define NSEC_PER_SEC	1000000000ULL
#define SYNC0_DC_EVT_PERIOD_NS	4000000	// SYNC0 compensation cycle, 4ms.
#define SYNC0_SHIFT	150000 // SYNC0 shift interval.
#define SYNC1_DC_EVT_PERIOD_NS	0	// SYNC1 compensation cycle, 4ms.
#define SYNC1_SHIFT	0 // SYNC1 shift interval.
#define DC_FILTER_CNT	1024

static uint64_t dc_start_time_ns = 0LL;
static uint64_t dc_time_ns = 0;
static uint8_t 	dc_started = 0;
static int32_t	dc_diff_ns = 0;
static int32_t	prev_dc_diff_ns = 0;
static int64_t	dc_diff_total_ns = 0LL;
static int64_t	dc_delta_total_ns = 0LL;
static int		dc_filter_idx = 0;
static int64_t	dc_adjust_ns = 0;
static int64_t	sys_time_base = 0LL;
static uint64_t	dc_first_app_time = 0LL;
unsigned long long 	frame_period_ns = 0LL;
uint64_t wakeup_time = 0LL;

void dc_init (void);
uint64_t sys_time_ns (void);
uint64_t cal_1st_sleep_time (void);
RTIME sys_time_2_cnt (uint64_t time);
RTIME cal_sleep_time (uint64_t wakeup_time);
void sync_dc (void);
void update_master_clock (void);

/*
 * Return the sign of a number
 * ie -1 for -ve value, 0 for 0, +1 for +ve value
 * \ret val the sign of the value
 */
#define sign(val) \
        ({ typeof (val) _val = (val); \
        ((_val > 0) - (_val < 0)); })
/* DC here */


/* Handle exception */
static unsigned int sig_alarms = 0;
void signal_handler(int signum) {
	switch (signum) {
	case SIGALRM:
		sig_alarms++;
		break;
	}
}

int first_sent = 0;
RT_TASK my_task;
void cleanup_all(void) {
	printf("delete my_task\n");
	rt_task_delete(&my_task);
	ecrt_release_master(master);
}

int run; /* Execution flag for a cyclic task. */
void catch_signal(int sig) {
	run = 0;

	// Wait until task stops
	rt_task_join(&my_task);

	cleanup_all();
	printf("exit by signal\n");
	exit(0);
	return;
}

/* For monitoring Working Counter changes. */
ec_domain_state_t domain_state = {};
void check_domain_state (ec_domain_t *domain)
{
	ec_domain_state_t ds;

	ecrt_domain_state(domain, &ds);

	if (ds.working_counter != domain_state.working_counter)
		rt_printf("Domain: WC %u.\n", ds.working_counter);
	if (ds.wc_state != domain_state.wc_state)
		rt_printf("Domain: State %u.\n", ds.wc_state);

	domain_state = ds;
}


void cia402_state_machine (int slv_num, unsigned short *sw, unsigned short *cw)
{
	if ((*sw & 0x4f) == 0x40) *cw = 0x06;//fprintf(stderr, "SwitchOnDisabled!!\n");
	else if ((*sw & 0x6f) == 0x21) *cw = 0x07; //fprintf(stderr, "Rdy2SwitchOn!!\n");
	else if ((*sw & 0x027f) == 0x233) *cw = 0x0f; //fprintf(stderr, "SwitchedOn!!\n");
	else if ((*sw & 0x027f) == 0x237) *cw = 0x1f; //fprintf(stderr, "Operation Enabled!!\n");
	else *cw = 0x80; //fprintf(stderr, "Unknown!!!!\n");
}

/* FIXME: Add variables if you add more slave devices. */
unsigned short statusWord0, controlWord0;
int actualPosition0, targetPosition0;

void __retrieve_0_0 ()
{
	if (first_sent) {
		// Retrieves datagram from ethercat frame(NIC driver->Master module).  
		ecrt_master_receive(master);
		// Processing datagram (Master module->domain).
		ecrt_domain_process(domainRd);
		ecrt_domain_process(domainWrt);

		/* FIXME: Add macro when you use more slave devices.*/
		/* Get TxPDO data from domain. */
		statusWord0 = EC_READ_U16(domainWrt_pd + slave0_6041_00);
		actualPosition0 = EC_READ_S32(domainWrt_pd + slave0_6064_00);
	}
}

static RTIME _last_occur=0;
static RTIME _last_publish=0;
RTIME _current_lag=0;
RTIME _max_jitter=0;

static inline RTIME max(RTIME a,RTIME b) {return a>b?a:b;}

void __publish_0_0 ()
{
	/* FIXME: Add macro when you use more slave devices.*/
	/* Write RxPDO to the domain */

	targetPosition0 = actualPosition0 + 10000; /* Set target position */

	EC_WRITE_U16(domainRd_pd + slave0_6040_00, controlWord0);
	EC_WRITE_S32(domainRd_pd + slave0_607a_00, targetPosition0);

	/* Queueing RxPDO to datagram (Domain->Master module). */
	ecrt_domain_queue(domainWrt);
	ecrt_domain_queue(domainRd);

#if 0
    {
        RTIME current_time = sys_time_ns();
        // Limit spining max 1/5 of common_ticktime
        RTIME maxdeadline = current_time + (cycle_ns / 5);
        RTIME deadline = _last_occur ? 
            _last_occur + cycle_ns : 
            current_time + _max_jitter; 
        if(deadline > maxdeadline) deadline = maxdeadline;
        _current_lag = deadline - current_time;
        if(_last_publish != 0){
            RTIME period = current_time - _last_publish;
            if(period > cycle_ns)
                _max_jitter = max(_max_jitter, period - cycle_ns);
            else
                _max_jitter = max(_max_jitter, cycle_ns - period);
        }
        _last_publish = current_time;
        _last_occur = current_time;
        while(current_time < deadline) {
            _last_occur = current_time; //Drift backward by default
            current_time = sys_time_ns();
        }
        if( _max_jitter * 10 < cycle_ns && _current_lag < _max_jitter){
            //Consuming security margin ?
            _last_occur = current_time; //Drift forward
        }
    }
#endif

	/* DC sync slaves */
	sync_dc ();

	/* Transmission EtherCAT frame */
	ecrt_master_send(master);

	/* Adjust master time based on reference clock time */
	update_master_clock();
	first_sent = 1;

}

/* Xenomai task body */
void my_task_proc(void *arg)
{
	int ret;
	
	run = 1;

	/* Set Xenomai task execution mode */
	ret = rt_task_set_mode(0, T_CONFORMING, NULL);
	if (ret) {
		rt_printf("error while rt_task_set_mode, code %d\n",ret);
		return;
	}

	/* To sync DC cycle based on reference clock value */
	RTIME wakeup_cnt;
	wakeup_time = cal_1st_sleep_time ();
	wakeup_cnt = cal_sleep_time (wakeup_time);
	rt_task_sleep_until (wakeup_cnt);

	/* Start pdo exchange loop until user stop */
	while (run) {

		__retrieve_0_0 ();
#if 0
		/* Not mandatory */
		check_domain_state (domainRd);
		check_domain_state (domainWrt);
#endif
		/* FIXME: Add following codes per each slave device..*/
		/* CiA 402 State machine according to the drive manual. (13-3 Devie control) */
		/* should be modified to multiple slaves. */
		cia402_state_machine (1, &statusWord0, &controlWord0);
	
		__publish_0_0 ();

		/* Wait until next release point */
		wakeup_time = wakeup_time + SM_FRAME_PERIOD_NS;
		wakeup_cnt = cal_sleep_time(wakeup_time);
		rt_task_sleep_until (wakeup_cnt);
	}
}

/*
 * Update the master time based on ref slaves time diff
 * called after the ecrt_master_send () to avoid time jitter in
 * sync_distributed_clocks()
 */
void update_master_clock(void)
{
    // calc drift (via un-normalised time diff)
    int32_t delta = dc_diff_ns - prev_dc_diff_ns;
    prev_dc_diff_ns = dc_diff_ns;

    // normalise the time diff
    dc_diff_ns = dc_diff_ns >= 0 ?
            ((dc_diff_ns + (int32_t)(frame_period_ns / 2)) %
                    (int32_t)frame_period_ns) - (frame_period_ns / 2) :
                    ((dc_diff_ns - (int32_t)(frame_period_ns / 2)) %
                            (int32_t)frame_period_ns) - (frame_period_ns / 2) ;

    // only update if primary master
    if (dc_started) {
        // add to totals
        dc_diff_total_ns += dc_diff_ns;
        dc_delta_total_ns += delta;
        dc_filter_idx++;

        if (dc_filter_idx >= DC_FILTER_CNT) {
            dc_adjust_ns += dc_delta_total_ns >= 0 ?
                    ((dc_delta_total_ns + (DC_FILTER_CNT / 2)) / DC_FILTER_CNT) :
                    ((dc_delta_total_ns - (DC_FILTER_CNT / 2)) / DC_FILTER_CNT) ;

            // and add adjustment for general diff (to pull in drift)
            dc_adjust_ns += sign(dc_diff_total_ns / DC_FILTER_CNT);

            // limit crazy numbers (0.1% of std cycle time)
            if (dc_adjust_ns < -1000) {
                dc_adjust_ns = -1000;
            }
            if (dc_adjust_ns > 1000) {
                dc_adjust_ns =  1000;
            }
            // reset
            dc_diff_total_ns = 0LL;
            dc_delta_total_ns = 0LL;
            dc_filter_idx = 0;
        }
        // add cycles adjustment to time base (including a spot adjustment)
        sys_time_base += dc_adjust_ns + sign(dc_diff_ns);
    }
    else {
        dc_started = (dc_diff_ns != 0);

        if (dc_started) {
#if 1
            // output first diff
            fprintf(stderr, "First master diff: %d\n", dc_diff_ns);
#endif
            // record the time of this initial cycle
            dc_start_time_ns = dc_time_ns;
        }
    }
}

void sync_dc (void)
{
	uint32_t ref_time = 0;
	RTIME prev_app_time = dc_time_ns;

	if (!ecrt_master_reference_clock_time (master, &ref_time)) {
		dc_diff_ns = (uint32_t) prev_app_time - ref_time;
	}

	ecrt_master_sync_slave_clocks (master);

	dc_time_ns = sys_time_ns ();
	ecrt_master_application_time (master, dc_time_ns);
}


/* Convert system time to Xenomai time in count via system_time_base.
*/
RTIME sys_time_2_cnt (uint64_t time)
{
	RTIME ret;

	if ((sys_time_base < 0) && ((uint64_t) (-sys_time_base) > time)) {
		fprintf (stderr, "%s() error: system_time base is less than \
						system time (system_time_base: %lld, time: %llu\n",
						__func__, sys_time_base, time);
		ret = time;
	}
	else {
		ret = time + sys_time_base;
	}

	return (RTIME) rt_timer_ns2ticks(ret);
}

RTIME cal_sleep_time (uint64_t wakeup_time)
{
	RTIME wakeup_cnt = sys_time_2_cnt(wakeup_time);
	RTIME current_cnt = rt_timer_read();

	if ((wakeup_cnt < current_cnt) || (wakeup_cnt > current_cnt + (50 * frame_period_ns))) {
		fprintf (stderr, "%s(): unexpected wake time!! \
			  	 wakeup count = %lld\t current count = %lld\n",
				 __func__, wakeup_cnt, current_cnt);
	}

	return wakeup_cnt;
}

/*
   Calculate the 1st sleep time to sync ref clock and master clock.
*/
uint64_t cal_1st_sleep_time (void)
{
	uint64_t dc_remainder = 0LL;
	uint64_t dc_phase_set_time = 0LL;

	dc_phase_set_time = sys_time_ns() + frame_period_ns * 50;
	dc_remainder = (dc_phase_set_time - dc_first_app_time) % frame_period_ns;

	return dc_phase_set_time + frame_period_ns - dc_remainder;
}

/* Get the time (ns) for the current CPU, 
   adjusted by system_time_base. 
   Rather than call rt_timer_read () directly, 
   Use this function instead. 
*/
uint64_t sys_time_ns(void)
{

	RTIME now = rt_timer_read();

	if (unlikely(sys_time_base > (SRTIME) now)) {
		fprintf (stderr, "%s() error: system_time base is greater than" 
						"system time (system_time_base: %lld, time: %llu\n",
						__func__, sys_time_base, now);
		return now;
	}
	else {
		return now - sys_time_base;
	}
}

void dc_init (void)
{
	/* Set DC compensation period, same as frame cycle */
	frame_period_ns = SM_FRAME_PERIOD_NS;

	/* Set initial master time */ 
	dc_start_time_ns = sys_time_ns();
	fprintf(stderr, "dc_start_time_ns: %lld\n", dc_start_time_ns);


	dc_time_ns = dc_start_time_ns;
	dc_first_app_time = dc_start_time_ns;

	ecrt_master_application_time (master, dc_start_time_ns);

}


static unsigned int cycle_ns = SM_FRAME_PERIOD_NS;

int main(int argc,char **argv)
{
	int ret;
	uint32_t abort_code;

	/* Lock all currently mapped pages to prevent page swapping */
	mlockall(MCL_CURRENT | MCL_FUTURE);

	/* Register signal handler */
	signal(SIGTERM, catch_signal);
	signal(SIGINT, catch_signal);

	/* Request EtherCAT master */
	master = ecrt_request_master(0);
	if (!master) return -1;

	/* Create domains for PDO exchange. */
	/* Yaskawa drive dose not handle LRW commands, 
	   thus we use separate domains. */
	domainRd = ecrt_master_create_domain(master);
	if (!domainRd) return -1;
	domainWrt = ecrt_master_create_domain(master);
	if (!domainWrt) return -1;
	
	/* PDO configuration for slave device */ 
	/* FIXME: Use following codes per slave device */
	if (!(slave0 = ecrt_master_slave_config(master, YAS_ALIAS, YASKAWA))) {
		fprintf(stderr, "Failed to get slave 0 and position 0.\n");
		return -1;
	}

	if (ecrt_slave_config_pdos(slave0, EC_END, slave_0_syncs)) {
		fprintf(stderr, "Failed to configure PDOs for slave 0 and position 0.\n");
		return -1;
	}
	// FIXME: Add here when you use more slave devices. 

	/* Setup EtherCAT Master transmit interval(us), 
	   usually same as the period of control task. */
	ecrt_master_set_send_interval(master, cycle_ns / 1000);

	/* Register pdo entry to the domains */
	if (ecrt_domain_reg_pdo_entry_list(domainRd, domainRd_regs)) {
		fprintf(stderr, "PDO entry registration failed!\n");
		return -1;
	}
	if (ecrt_domain_reg_pdo_entry_list(domainWrt, domainWrt_regs)) {
		fprintf(stderr, "PDO entry registration failed!\n");
		return -1;
	}

	/* Setup Drive operation mode via SDO.*/
	/* Default value of received drive is 9 now. */
	{
		/* FIXME: Use following codes when multiple drives are required. */
		uint8_t value[1];
		EC_WRITE_S8((uint8_t *)value, 0x08);
		if(ecrt_master_sdo_download(master, 0, 0x6060, 0x00, (uint8_t *)value, 1, &abort_code))
		{
			fprintf(stderr, "Failed to initialize slave 0 and position 0,\nError: %d\n", abort_code);
			return -1;
		}
	}

	/* Configuring DC signal */
	ecrt_slave_config_dc (slave0, 0x0300, SYNC0_DC_EVT_PERIOD_NS, SYNC0_SHIFT,
										  SYNC1_DC_EVT_PERIOD_NS, SYNC1_SHIFT);

	/* Select ref. clock */
	if (ecrt_master_select_reference_clock (master, slave0)) {
		fprintf (stderr, "Failed to select reference clock!!!!\n");
		return -1;
	}

	dc_init ();

	/* Activating master stack */
	if (ecrt_master_activate(master))
		return -1;

	/* Debugging purpose, get the address of mapped domains. */
	if (!(domainRd_pd = ecrt_domain_data(domainRd))) return -1;
	if (!(domainWrt_pd = ecrt_domain_data(domainWrt))) return -1;
	fprintf(stdout, "Master 0 activated...\n\n");
	fprintf(stdout, "domainRd_pd:  0x%.6lx\n", (unsigned long)domainRd_pd);
	fprintf(stdout, "domainWrt_pd:  0x%.6lx\n", (unsigned long)domainWrt_pd);

	/* Creating cyclic xenomai task */
	ret = rt_task_create(&my_task,"my_task",0,50,T_JOINABLE);
	if (ret) {
		fprintf (stderr, "Task create failed!!!!\n");
	}

	/* Starting cyclic task */
	fprintf(stdout, "starting my_task\n");
	ret = rt_task_start(&my_task, &my_task_proc, NULL);
	if (ret) {
		fprintf (stderr, "Task start failed!!!!\n");
	}

	while (run)	{
		sched_yield();
	}

	/* Cleanup routines. */ 
	rt_task_join(&my_task);
	rt_task_delete(&my_task);
	fprintf(stdout, "End of Program\n");
	ecrt_release_master(master); /* Releases a requested EtherCAT master */

	return 0;
}
