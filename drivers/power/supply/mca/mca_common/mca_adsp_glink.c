#include <linux/module.h>
#include <linux/types.h>
#include <linux/soc/qcom/qti_pmic_glink.h>
#include <mca/common/mca_adsp_glink.h>

#ifndef MCA_LOG_TAG
#define MCA_LOG_TAG "mca_adsp_glink"
#endif

#define MCA_ADSP_GLINK_OWNER 0x800A
#define MCA_ADSP_GLINK_MSG_REQ 1 /* hdr.type: request/response */
#define MCA_ADSP_GLINK_MSG_NOTIFY 2 /* hdr.type: async notification */
#define MCA_ADSP_GLINK_OPCODE_READ 1
#define MCA_ADSP_GLINK_OPCODE_WRITE 2
#define MCA_ADSP_PROP_DATA_LEN 256
#define MCA_ADSP_GLINK_TIMEOUT_MS 250

struct mca_adsp_glink_resp_msg {
	struct pmic_glink_hdr hdr;
	u32 property_id;
	u32 retcode;
	u32 seq;
	u8 data[MCA_ADSP_PROP_DATA_LEN];
} __packed;

struct mca_adsp_glink_notify_msg {
	struct pmic_glink_hdr hdr;
	u32 property_id;
	u8 data[MCA_ADSP_PROP_DATA_LEN];
} __packed;

struct mca_adsp_glink_dev {
	struct device *dev;
	struct pmic_glink_client *client;
	struct mutex rw_lock;
	struct completion ack;
	int glink_state;
	struct work_struct sync_work;
	u32 cur_property_id;
	int retcode;
	u32 seq;
	u8 read_buf[MCA_ADSP_PROP_DATA_LEN];
};

static struct mca_adsp_glink_dev *g_mca_adsp_glink;
static LIST_HEAD(g_mca_adsp_ops_list);

int mca_adsp_glink_write_prop(int prop_id, void *value, size_t size)
{
	return 0;
}
EXPORT_SYMBOL(mca_adsp_glink_write_prop);

int mca_adsp_glink_read_prop(int prop_id, void *value, size_t size)
{
	return 0;
}
EXPORT_SYMBOL(mca_adsp_glink_read_prop);

int mca_adsp_glink_register_ops(struct mca_adsp_glink_ops *ops, void *priv)
{
	return 0;
}
EXPORT_SYMBOL(mca_adsp_glink_register_ops);

static int
mca_adsp_glink_handle_notification(struct mca_adsp_glink_dev *mca,
				   struct mca_adsp_glink_notify_msg *msg)
{
	return 0;
}

static int mca_adsp_glink_handle_message(struct mca_adsp_glink_dev *mca,
					 struct mca_adsp_glink_resp_msg *msg,
					 size_t len)
{
	return 0;
}

static int mca_adsp_glink_callback(void *priv, void *data, size_t len)
{
	return 0;
}

static void mca_adsp_glink_sync_work(struct work_struct *work)
{
	return 0;
}

static void mca_adsp_glink_state_cb(void *priv, enum pmic_glink_state state)
{
	return 0;
}

static int mca_adsp_glink_probe(struct platform_device *pdev)
{
	struct mca_adsp_glink_dev *mca;
	struct pmic_glink_client_data client_data = { 0 };
	int ret;

	mca = devm_kzalloc(&pdev->dev, sizeof(*mca), GFP_KERNEL);
	if (!mca)
		return -ENOMEM;

	mca->dev = &pdev->dev;
	mca->cur_property_id = 0xffffffff;
	mutex_init(&mca->rw_lock);
	init_completion(&mca->ack);
	mca->glink_state = 1;
	INIT_WORK(&mca->sync_work, mca_adsp_glink_sync_work);

	client_data.name = "mca_adap_glink";
	client_data.id = MCA_ADSP_GLINK_OWNER;
	client_data.priv = mca;
	client_data.msg_cb = mca_adsp_glink_callback;
	client_data.state_cb = mca_adsp_glink_state_cb;

	mca->client = pmic_glink_register_client(&pdev->dev, &client_data);
	if (IS_ERR(mca->client)) {
		ret = PTR_ERR(mca->client);
		if (ret == -EPROBE_DEFER)
			return ret;
		mca_log_err("Error in registering with pmic_glink %d\n", ret);
		return ret;
	}

	platform_set_drvdata(pdev, mca);
	g_mca_adsp_glink = mca;
	mca_log_err("probe ok\n");
	return 0;
}

static int mca_adsp_glink_remove(struct platform_device *pdev)
{
	return 0;
}

static const struct of_device_id match_table[] = {
	{ .compatible = "mca,adsp_glink" },
	{},
};

static struct platform_driver mca_adsp_driver = {
	.driver = {
		.name = "mca_adsp_glink",
		.of_match_table = match_table,
	},
	.probe = mca_adsp_glink_probe,
	.remove = mca_adsp_glink_remove,
};
module_platform_driver(mca_adsp_driver);

MODULE_DESCRIPTION("mca glink driver");
MODULE_AUTHOR("liyuze1@xiaomi.com");
MODULE_LICENSE("GPL v2");
