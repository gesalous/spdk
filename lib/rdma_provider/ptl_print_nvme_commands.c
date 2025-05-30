#include "ptl_print_nvme_commands.h"
#include "ptl_log.h"
#include "spdk/nvme.h"
#include "spdk/nvme_spec.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
// Function to print NVMe command in human-readable format
void ptl_print_nvme_cmd(const struct spdk_nvme_cmd *cmd, const char *prefix)
{
	const char *command;
	if (!cmd) {
		SPDK_PTL_DEBUG("NVMe-cmd: NULL command");
		return;
	}
	// Interpret common opcodes
	switch (cmd->opc) {
	case SPDK_NVME_OPC_READ:
		command = "READ";
		break;
	case SPDK_NVME_OPC_WRITE:
		command = "WRITE";
		break;
	case SPDK_NVME_OPC_FLUSH:
		command = "FLUSH";
		break;
	case SPDK_NVME_OPC_IDENTIFY:
		command = "IDENTIFY";
		break;

	/* 0x03 - reserved */
	case SPDK_NVME_OPC_WRITE_UNCORRECTABLE:
		command = "WRITE_UNCORRECTABLE";
		break;
	case SPDK_NVME_OPC_COMPARE:
		command = "OPC_COMPARE";
		break;
	case SPDK_NVME_OPC_WRITE_ZEROES:
		command = "OPC_WRITE_ZEROES";
		break;
	case SPDK_NVME_OPC_DATASET_MANAGEMENT:
		command = "DATASET_MANAGEMENT";
		break;
	case SPDK_NVME_OPC_VERIFY:
		command = "OPC_VERIFY";
		break;
	case SPDK_NVME_OPC_RESERVATION_REGISTER:
		command = "RESERVATION_REGISTER";
		break;
	case SPDK_NVME_OPC_RESERVATION_REPORT:
		command = "RESERVATION_REPORT";
		break;
	case SPDK_NVME_OPC_RESERVATION_ACQUIRE:
		command = "RESERVATION_ACQUIRE";
		break;
	case SPDK_NVME_OPC_IO_MANAGEMENT_RECEIVE:
		command = "MANAGEMENT_RECEIVE";
		break;
	case SPDK_NVME_OPC_RESERVATION_RELEASE:
		command = "OPC_RESERVATION_RELEASE";
		break;
	case SPDK_NVME_OPC_COPY:
		command = "OPC_COPY";
		break;
	case SPDK_NVME_OPC_IO_MANAGEMENT_SEND:
		command = "OPC_IO_MANAGEMET";
		break;
	default:
		command = "UNKNOWN";
		break;
	}

	SPDK_PTL_DEBUG("%s: command opcode: 0x%02x  human-readable: %s", prefix, cmd->opc, command);
	SPDK_PTL_DEBUG("%s: NSID: %u",prefix, cmd->nsid);
	SPDK_PTL_DEBUG("%s: CID: %u",prefix, cmd->cid);
	SPDK_PTL_DEBUG("%s:  FUSE: %u", prefix, cmd->fuse);
	SPDK_PTL_DEBUG("%s:  PSDT: %u\n",prefix, cmd->psdt);
	SPDK_PTL_DEBUG("%s:  CDW10-15: 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x",prefix,
		       cmd->cdw10, cmd->cdw11, cmd->cdw12, cmd->cdw13, cmd->cdw14, cmd->cdw15);
}

// Function to print NVMe completion in human-readable format
void ptl_print_nvme_cpl(const struct spdk_nvme_cpl *cpl, const char *prefix)
{
	if (!cpl) {
		SPDK_PTL_DEBUG("NVMe-cpl: NULL completion");
		return;
	}

	SPDK_PTL_DEBUG("NVMe-cpl: Completion Status: 0x%04x", cpl->status_raw);

	// Interpret status code
	if (cpl->status.sct == SPDK_NVME_SCT_GENERIC) {
		SPDK_PTL_DEBUG("%s:  Status Code Type: Generic (0x%x)", prefix, cpl->status.sct);
		switch (cpl->status.sc) {
		case SPDK_NVME_SC_SUCCESS:
			SPDK_PTL_DEBUG("%s:  Status Code: Success (0x%x)", prefix, cpl->status.sc);
			break;
		case SPDK_NVME_SC_INVALID_OPCODE:
			SPDK_PTL_DEBUG("%s:  Status Code: Invalid Opcode (0x%x)", prefix, cpl->status.sc);
			break;
		case SPDK_NVME_SC_INVALID_FIELD:
			SPDK_PTL_DEBUG("%s:  Status Code: Invalid Field (0x%x)", prefix, cpl->status.sc);
			break;
		default:
			SPDK_PTL_DEBUG("%s:  Status Code: Unknown (0x%x)", prefix, cpl->status.sc);
			break;
		}
	} else {
		SPDK_PTL_DEBUG("%s:  Status Code Type: 0x%x", prefix, cpl->status.sct);
		printf("  Status Code: 0x%x\n", cpl->status.sc);
	}

	SPDK_PTL_DEBUG("%s:  Phase: %u", prefix, cpl->status.p);
	SPDK_PTL_DEBUG("%s:  CID: %u", prefix,cpl->cid);
	SPDK_PTL_DEBUG("%s:  SQID: %u", prefix, cpl->sqid);
	SPDK_PTL_DEBUG("%s:  SQHD: %u", prefix, cpl->sqhd);
	SPDK_PTL_DEBUG("%s:  Result Data: 0x%08x", prefix, cpl->cdw0);
}

