#include <switchtec/switchtec.h>
#include <stdint.h>

struct switchtec_lnkerr_dllp_in {
	uint8_t subcmd;
	uint8_t phys_port_id;
	uint8_t resvd[2];
	uint32_t data;
};

struct switchtec_lnkerr_dllp_crc_in {
	uint8_t subcmd;
	uint8_t phys_port_id;
	uint8_t enable;
	uint8_t resvd1;
	uint16_t rate;
	uint8_t resvd2[2];
};

struct switchtec_lnkerr_tlp_lcrc_gen5_in {
	uint8_t subcmd;
	uint8_t phys_port_id;
	uint8_t enable;
	uint8_t resvd1;
	uint8_t rate;
	uint8_t resvd[3];
};

struct switchtec_lnkerr_tlp_lcrc_gen4_in {
	uint8_t subcmd;
	uint8_t phys_port_id;
	uint8_t enable;
	uint8_t rate;
};

struct switchtec_lnkerr_tlp_seqn_in {
	uint8_t subcmd;
	uint8_t phys_port_id;
	uint8_t resvd[2];
};

struct switchtec_lnkerr_ack_nack_in {
	uint8_t subcmd;
	uint8_t phys_port_id;
	uint8_t resvd1[2];
	uint16_t seq_num;
	uint8_t count;
	uint8_t resvd2;
};

struct switchtec_lnkerr_cto_in {
	uint8_t subcmd;
	uint8_t phys_port_id;
	uint8_t resvd[2];
};
