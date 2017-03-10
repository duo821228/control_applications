#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/mman.h>
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
#define MINAS_ALIAS				0,1 // FIXME: This macro should be declared per slave.
#define SAN_ALIAS				0,2 // FIXME: This macro should be declared per slave.

#define YASKAWA					0x00000539, 0x02200301 // VendorId, ProductID
#define MINAS					0x0000066f, 0x525100a1 // VendorId, ProductID
#define SANYO					0x000001b9, 0x00000002 // VendorId, ProductID

/* EtherCAT Configuration pointer */
static ec_master_t *master = NULL;

/* FIXME:  Declares per slave device. */
static ec_slave_config_t *slave0 = NULL;
static ec_slave_config_t *slave1 = NULL;
static ec_slave_config_t *slave2 = NULL;

/* FIXME: Process Data Offset in the domain. 
   Declares per slave device. */

/* Yaskawa drive */
unsigned int slave0_6040_00;
unsigned int slave0_607a_00;
unsigned int slave0_6041_00;
unsigned int slave0_6064_00;

/* Panasonic drive */
unsigned int slave1_6041_00;
unsigned int slave1_6061_00;
unsigned int slave1_6064_00;
unsigned int slave1_6040_00;
unsigned int slave1_6060_00;
unsigned int slave1_60ff_00;
unsigned int slave1_607a_00;
unsigned int slave1_60b8_00;

/* Sanyo drive */
unsigned int slave2_6041_00;
unsigned int slave2_6064_00;
unsigned int slave2_606c_00;
unsigned int slave2_6077_00;
unsigned int slave2_6061_00;
unsigned int slave2_6040_00;
unsigned int slave2_607a_00;
unsigned int slave2_6081_00;
unsigned int slave2_6083_00;
unsigned int slave2_6084_00;
unsigned int slave2_60ff_00;
unsigned int slave2_6071_00;
unsigned int slave2_60b8_00;
unsigned int slave2_60fe_01;
unsigned int slave2_6060_00;


/* Yaskawa drive */
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

/* Slave 1, "MBDHT2510BD1"
 * Vendor ID:       0x0000066f
 * Product code:    0x525100a1
 * Revision number: 0x00010000
 */

/* Panasonic drive */
ec_pdo_entry_info_t slave_1_pdo_entries[] = {
    {0x6040, 0x00, 16}, /* Controlword */
    {0x6060, 0x00, 8}, /* Modes of operation */
    {0x607a, 0x00, 32}, /* Target position */
    {0x60b8, 0x00, 16}, /* Touch probe function */
    {0x603f, 0x00, 16}, /* Error code */
    {0x6041, 0x00, 16}, /* Statusword */
    {0x6061, 0x00, 8}, /* Modes of operation display */
    {0x6064, 0x00, 32}, /* Position actual value */
    {0x60b9, 0x00, 16}, /* Touch probe status */
    {0x60ba, 0x00, 32}, /* Touch probe pos1 pos value */
    {0x60f4, 0x00, 32}, /* Following error actual value */
    {0x60fd, 0x00, 32}, /* Digital inputs */
};

ec_pdo_info_t slave_1_pdos[] = {
    {0x1600, 4, slave_1_pdo_entries + 0}, /* Receive PDO mapping 1 */
    {0x1a00, 8, slave_1_pdo_entries + 4}, /* Transmit PDO mapping 1 */
};

ec_sync_info_t slave_1_syncs[] = {
    {0, EC_DIR_OUTPUT, 0, NULL, EC_WD_DISABLE},
    {1, EC_DIR_INPUT, 0, NULL, EC_WD_DISABLE},
    {2, EC_DIR_OUTPUT, 1, slave_1_pdos + 0, EC_WD_ENABLE},
    {3, EC_DIR_INPUT, 1, slave_1_pdos + 1, EC_WD_DISABLE},
    {0xff}
};

/* Sanyo drive */
ec_pdo_entry_info_t slave_2_pdo_entries[] = {
		/* Tx */
    {0x6040, 0x00, 16}, /* Control word */
    {0x607a, 0x00, 32}, /* Target position */
    {0x6081, 0x00, 32}, /* Profile velocity */
    {0x6083, 0x00, 32}, /* Profile acceleration */
    {0x6084, 0x00, 32}, /* Profile deceleration */
    {0x60ff, 0x00, 32}, /* Target velocity */
    {0x6071, 0x00, 16}, /* Target torque */
    {0x60b8, 0x00, 16}, /* Touch probe function */
    {0x60fe, 0x01, 32}, /* Digital outputs */
		/* Rx */
    {0x6041, 0x00, 16}, /* Status word */
    {0x2100, 0x00, 16}, /* Status word 1 */
    {0x6064, 0x00, 32}, /* Position actual value */
    {0x606c, 0x00, 32}, /* Velocity actual value */
    {0x6077, 0x00, 16}, /* Torque actual value */
    {0x60f4, 0x00, 32}, /* Following error actualvalue */
    {0x60b9, 0x00, 16}, /* Touch probe status */
    {0x60ba, 0x00, 32}, /* Touch probe position 1 positive value */
    {0x60bb, 0x00, 32}, /* Touch probe position 1 negative value */
    {0x60fd, 0x00, 32}, /* Digital input */
    {0x1001, 0x00, 8}, /* Error register */
    {0x6061, 0x00, 8}, /* Modes of operation display */
};

ec_pdo_info_t slave_2_pdos[] = {
    {0x1700, 9, slave_2_pdo_entries + 0}, /* Outputs */
    {0x1b00, 12, slave_2_pdo_entries + 9}, /* Inputs */
};

ec_sync_info_t slave_2_syncs[] = {
    {0, EC_DIR_OUTPUT, 0, NULL, EC_WD_DISABLE},
    {1, EC_DIR_INPUT, 0, NULL, EC_WD_DISABLE},
    {2, EC_DIR_OUTPUT, 1, slave_2_pdos + 0, EC_WD_DISABLE},
    {3, EC_DIR_INPUT, 1, slave_2_pdos + 1, EC_WD_DISABLE},
    {0xff}
};


/* Separate two domains, yaskawa drive requires separated domain. */
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
    {MINAS_ALIAS, MINAS, 0x6040, 0, &slave1_6040_00},
    {MINAS_ALIAS, MINAS, 0x6060, 0, &slave1_6060_00},
    {MINAS_ALIAS, MINAS, 0x607a, 0, &slave1_607a_00},
    {MINAS_ALIAS, MINAS, 0x60b8, 0, &slave1_60b8_00},
	{SAN_ALIAS, SANYO, 0x6040, 0, &slave2_6040_00},
	{SAN_ALIAS, SANYO, 0x607a, 0, &slave2_607a_00},
	{SAN_ALIAS, SANYO, 0x6081, 0, &slave2_6081_00},
	{SAN_ALIAS, SANYO, 0x6083, 0, &slave2_6083_00},
	{SAN_ALIAS, SANYO, 0x6084, 0, &slave2_6084_00},
	{SAN_ALIAS, SANYO, 0x60ff, 0, &slave2_60ff_00},
	{SAN_ALIAS, SANYO, 0x6071, 0, &slave2_6071_00},
	{SAN_ALIAS, SANYO, 0x60b8, 0, &slave2_60b8_00},
	{SAN_ALIAS, SANYO, 0x60fe, 1, &slave2_60fe_01},
	{}
};

const static ec_pdo_entry_reg_t domainWrt_regs[] = {
	/* TxPDOs */
	{YAS_ALIAS, YASKAWA, 0x6041, 0, &slave0_6041_00},
	{YAS_ALIAS, YASKAWA, 0x6064, 0, &slave0_6064_00},
    {MINAS_ALIAS, MINAS, 0x6041, 0, &slave1_6041_00},
    {MINAS_ALIAS, MINAS, 0x6061, 0, &slave1_6061_00},
    {MINAS_ALIAS, MINAS, 0x6064, 0, &slave1_6064_00},
	{SAN_ALIAS, SANYO, 0x6041, 0, &slave2_6041_00},
	{SAN_ALIAS, SANYO, 0x6064, 0, &slave2_6064_00},
	{SAN_ALIAS, SANYO, 0x6061, 0, &slave2_6061_00},
	{}
};

/* Handle exception */
static unsigned int sig_alarms = 0;
void signal_handler(int signum) {
	switch (signum) {
	case SIGALRM:
		sig_alarms++;
		break;
	}
}

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

/* Xenomai task body */
void my_task_proc(void *arg)
{
	int ret;
	
	unsigned short statusWord0, controlWord0;
	unsigned short statusWord1, controlWord1;
	unsigned short statusWord2, controlWord2;
	int actualPosition0, targetPosition0;
	int actualPosition1, targetPosition1;
	int actualPosition2, targetPosition2;
	
	run = 1;

	/* Set Xenomai task execution mode */
	ret = rt_task_set_mode(0, T_CONFORMING, NULL);
	if (ret) {
		rt_printf("error while rt_task_set_mode, code %d\n",ret);
		return;
	}
	/* Periodic execution */
	rt_task_set_periodic(NULL, TM_NOW, SM_FRAME_PERIOD_NS);
	
	/* Start pdo exchange loop until user stop */
	while (run) {
		// Retrieves datagram from ethercat frame(NIC driver->Master module).  
		ecrt_master_receive(master);
		// Processing datagram (Master module->domain).
		ecrt_domain_process(domainRd);
		ecrt_domain_process(domainWrt);

		/* FIXME: Add macro when you use more slave devices.*/
		/* Get TxPDO data from domain. */
		statusWord0 = EC_READ_U16(domainWrt_pd + slave0_6041_00);
		actualPosition0 = EC_READ_S32(domainWrt_pd + slave0_6064_00);
		statusWord1 = EC_READ_U16(domainWrt_pd + slave1_6041_00);
		actualPosition1 = EC_READ_S32(domainWrt_pd + slave1_6064_00);
		statusWord2 = EC_READ_U16(domainWrt_pd + slave2_6041_00);
		actualPosition2 = EC_READ_S32(domainWrt_pd + slave2_6064_00);

		/* Not mandatory */
		check_domain_state (domainRd);
		check_domain_state (domainWrt);

		/* CiA 402 State machine according to the drive manual. */
		/* Yaskawa (13-3 Devie control) */
		if ((statusWord0 & 0x4f) == 0x40)	controlWord0 = 0x06; //fprintf(stderr, "SwitchOnDisabled!!\n");
		else if ((statusWord0 & 0x6f) == 0x21) controlWord0 = 0x07; //fprintf(stderr, "Rdy2SwitchOn!!\n");
		else if ((statusWord0 & 0x027f) == 0x233) controlWord0 = 0x0f; //fprintf(stderr, "SwitchedOn!!\n");
		else if ((statusWord0 & 0x027f) == 0x237) controlWord0 = 0x1f; //fprintf(stderr, "Operation Enabled!!\n");
		else controlWord0 = 0x80; //fprintf(stderr, "Unknown!!!!\n");

		/* Panasonic */
		if (statusWord1 == 592 || statusWord1 == 624) controlWord1 = 6; //fprintf(stderr, "SwitchOnDisabled!!\n");
		else if (statusWord1 == 561) controlWord1 = 7; //fprintf(stderr, "Rdy2SwitchOn!!\n");
		else if (statusWord1 == 563) controlWord1 = 15; //fprintf(stderr, "SwitchedOn!!\n");
		//else if (statusWord == 55 || statusWord == 1079) ;
		else if (statusWord1 == 567) controlWord1 = 15; //fprintf(stderr, "Operation Enabled!!\n");
		else controlWord1 = 128; // Other initial state.

		/* Sanyo */
		if (statusWord2 == 0x1437) continue; //fprintf(stderr, "Operation enabled!\n");
		else if (statusWord2 == 0x0450) controlWord2 = 0x06;//fprintf(stderr, "Switch on Disabled!\n");
		else if (statusWord2 == 0x0431) controlWord2 = 0x07;//fprintf(stderr, "Ready to Switch on !\n");
		else if (statusWord2 == 0x0433) controlWord2 = 0x1f;//fprintf(stderr, "Ready to Switch on !\n");

		targetPosition0 = actualPosition0 + 10000; /* Set target position */
		targetPosition1 = actualPosition1 + 10000; /* Set target position */

		/* Write RxPDO to the domain */
		EC_WRITE_U16(domainRd_pd + slave0_6040_00, controlWord0);
		EC_WRITE_S32(domainRd_pd + slave0_607a_00, targetPosition0);
		EC_WRITE_U16(domainRd_pd + slave1_6040_00, controlWord1);
		EC_WRITE_S32(domainRd_pd + slave1_607a_00, targetPosition1);
		// Sanyo operates CSV mode. 
		EC_WRITE_U16(domainRd_pd + slave2_6040_00, controlWord2);
		EC_WRITE_S32(domainRd_pd + slave2_60ff_00, 0x000FFFFF); 

		/* Queueing RxPDO to datagram (Domain->Master module). */
		ecrt_domain_queue(domainWrt);
		ecrt_domain_queue(domainRd);
		/* Transmission EtherCAT frame */
		ecrt_master_send(master);
		/* Wait until next release point */
		rt_task_wait_period(NULL);
	}
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
	if (!(slave1 = ecrt_master_slave_config(master, MINAS_ALIAS, MINAS))) {
		fprintf(stderr, "Failed to get slave 1 and position 0.\n");
		return -1;
	}
	if (!(slave2 = ecrt_master_slave_config(master, SAN_ALIAS, SANYO))) {
		fprintf(stderr, "Failed to get slave 2 and position 0.\n");
		return -1;
	}


	if (ecrt_slave_config_pdos(slave0, EC_END, slave_0_syncs)) {
		fprintf(stderr, "Failed to configure PDOs for slave 0 and position 0.\n");
		return -1;
	}
	if (ecrt_slave_config_pdos(slave1, EC_END, slave_1_syncs)) {
		fprintf(stderr, "Failed to configure PDOs for slave 1 and position 0.\n");
		return -1;
	}
	if (ecrt_slave_config_pdos(slave2, EC_END, slave_2_syncs)) {
		fprintf(stderr, "Failed to configure PDOs for slave 2 and position 0.\n");
		return -1;
	}

	/* Setup EtherCAT Master transmit interval(us), 
	   usually same as the period of control task. */
	ecrt_master_set_send_interval(master, cycle_ns / 1000);

	/* Register pdo entry to the domains */
	if (ecrt_domain_reg_pdo_entry_list(domainRd, domainRd_regs)) {
		fprintf(stderr, "PDO RD entry registration failed!\n");
		return -1;
	}
	if (ecrt_domain_reg_pdo_entry_list(domainWrt, domainWrt_regs)) {
		fprintf(stderr, "PDO Wrt entry registration failed!\n");
		return -1;
	}

	/* Setup Drive operation mode via SDO.*/
	/* Default value of received drive is 9 now. */
	{
		uint8_t value[1];

		for (int idx = 0; idx < 3; idx++) {
			if (idx != 2)
				EC_WRITE_S8((uint8_t *)value, 0x08);
			else
				EC_WRITE_S8((uint8_t *)value, 0x09); // Sanyo operation mode; CSV

			if (ecrt_master_sdo_download(master, idx, 0x6060, 0x00, (uint8_t *)value, 1, &abort_code)) {
				fprintf(stderr, "Failed to initialize slave %d and position %d,\nError: %d\n", idx, idx, abort_code);
				return -1;
			}
		}
#if 0
		if (ecrt_master_sdo_download(master, 0, 0x6060, 0x00, (uint8_t *)value, 1, &abort_code))
		{
			fprintf(stderr, "Failed to initialize slave 0 and position 0,\nError: %d\n", abort_code);
			return -1;
		}
		if (ecrt_master_sdo_download(master, 1, 0x6060, 0x00, (uint8_t *)value, 1, &abort_code))
		{
			fprintf(stderr, "Failed to initialize slave 1 and position 0,\nError: %d\n", abort_code);
			return -1;
		}
		// Sanyo operation mode : CSV
		EC_WRITE_S8((uint8_t *)value, 0x09);
		if(ecrt_master_sdo_download(master, 2, 0x6060, 0x00, (uint8_t *)value, 1, &abort_code))
		{
			fprintf(stderr, "Failed to initialize slave 2 and position 0,\nError: %d\n", abort_code);
			return -1;
		}
#endif
	}

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

	/* Starting cyclic task */
	fprintf(stdout, "starting my_task\n");
	ret = rt_task_start(&my_task, &my_task_proc, NULL);

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
