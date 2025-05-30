#ifndef PTL_PRINT_NVME_COMMANDS_H
#define PTL_PRINT_NVME_COMMANDS_H
struct spdk_nvme_cmd;
struct spdk_nvme_cpl;
// Function to print NVMe command in human-readable format
void ptl_print_nvme_cmd(const struct spdk_nvme_cmd *cmd, const char *prefix);


// Function to print NVMe completion in human-readable format
void ptl_print_nvme_cpl(const struct spdk_nvme_cpl *cpl, const char *prefix);
#endif
