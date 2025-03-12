#include "ptl_pd.h"
#include "ptl_log.h"
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

bool ptl_pd_add_mem_desc(struct ptl_pd *ptl_pd, ptl_handle_md_t mem_handle, ptl_md_t mem_desc)
{
	if (ptl_pd->num_ptl_mem_desc >= PTL_PD_MAX_MEM_DESC) {
		SPDK_PTL_FATAL("Sorry no room to add another portals memory descriptor");
		return false;
	}
	ptl_pd->ptl_mem_desc[ptl_pd->num_ptl_mem_desc].mem_handle = mem_handle;
	ptl_pd->ptl_mem_desc[ptl_pd->num_ptl_mem_desc].mem_desc = mem_desc;
	ptl_pd->ptl_mem_desc[ptl_pd->num_ptl_mem_desc++].is_valid = true;
	return true;
}

struct ptl_mem_desc ptl_pd_get_mem_desc(struct ptl_pd *ptl_pd, uint64_t address,
					size_t length)
{
	struct ptl_mem_desc not_found = {0};
	uint32_t i;
	uint64_t end_address = address + length;
	for (i = 0; i < ptl_pd->num_ptl_mem_desc; i++) {
		if ((uint64_t)ptl_pd->ptl_mem_desc[i].mem_desc.start <= address &&
		    (uint64_t)end_address <= ptl_pd->ptl_mem_desc[i].mem_desc.start +
		    ptl_pd->ptl_mem_desc[i].mem_desc.length) {
			SPDK_PTL_DEBUG("Found mem desc for portals!");
			return ptl_pd->ptl_mem_desc[i];
		}
	}
	SPDK_PTL_FATAL("OOPSIE! Not found analogous memory descriptor!");
	return not_found;
}
