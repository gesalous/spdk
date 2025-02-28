#include "ptl_pd.h"
#include "lib/rdma_provider/portals_log.h"
#include "ptl_object_types.h"

struct ptl_pd *ptl_pd_create(struct ptl_context *ptl_context)
{
	struct ptl_pd *ptl_pd = calloc(1UL, sizeof(*ptl_pd));
	if (NULL == ptl_pd) {
		SPDK_PTL_FATAL("Failed to allocate memory for portalds pd");
	}
	ptl_pd->object_type = PTL_PD;
	ptl_pd->ptl_cnxt = ptl_context;
	return ptl_pd;
}

bool ptl_pd_add_mem_desc(struct ptl_pd *ptl_pd, ptl_handle_md_t mem_desc, void *vaddr, uint32_t size)
{
	if (ptl_pd->num_ptl_mem_desc >= PTL_PD_MAX_MEM_DESC) {
		SPDK_PTL_FATAL("Sorry no room to add another portals memory descriptor");
		return false;
	}
	ptl_pd->ptl_mem_desc[ptl_pd->num_ptl_mem_desc].size = size;
	ptl_pd->ptl_mem_desc[ptl_pd->num_ptl_mem_desc].vaddr = vaddr;
	ptl_pd->ptl_mem_desc[ptl_pd->num_ptl_mem_desc++].mem_desc = mem_desc;
	return true;
}
