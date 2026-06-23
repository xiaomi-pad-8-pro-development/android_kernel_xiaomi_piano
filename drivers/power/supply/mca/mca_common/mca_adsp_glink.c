#include <linux/module.h>
#include <linux/types.h>
#include <mca/common/mca_adsp_glink.h>

int mca_adsp_glink_write_prop(int prop_id, void *value, size_t size)
{
	return 0;
}
EXPORT_SYMBOL(mca_adsp_glink_write_prop);

MODULE_DESCRIPTION("MCA ADSP glink (stub)");
MODULE_LICENSE("GPL v2");
