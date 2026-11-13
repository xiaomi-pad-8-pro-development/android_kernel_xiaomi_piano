ifneq ($(CONFIG_ARCH_QTI_VM), y)
dtbo-$(CONFIG_ARCH_ART) += display/art-sde.dtbo \
		display/art-sde-display-rumi-overlay.dtbo \
		display/art-sde-display-cdp-overlay.dtbo \
		display/art-sde-display-mtp-fig-st54l-overlay.dtbo \
		display/art-sde-display-mtp-fig-bt-st54l-overlay.dtbo \
		display/art-sde-display-mtp-fig-qmp1000-3.5mm-overlay.dtbo \
		display/art-sde-display-mtp-fig-pro-overlay.dtbo \
		display/art-sde-display-mtp-peach-st54l-overlay.dtbo \
		display/art-sde-display-mtp-peach-qmp1000-overlay.dtbo \
		display/art-sde-display-omtp-overlay.dtbo \
		display/art-sde-display-rcm-fig-overlay.dtbo \
		display/art-sde-display-rcm-peach-sn300u-overlay.dtbo \
		display/art-sde-display-qrd-sku1-overlay.dtbo \
		display/art-sde-display-qrd-sku2-overlay.dtbo \
		display/art-sde-display-qrd-sku2-185hz-overlay.dtbo \
		display/art-sde-display-qrd-sku2-165hz-overlay.dtbo \
		display/art-sde-display-hdk-overlay.dtbo \
		display/artl-sde.dtbo \
		display/artl-sde-display-mtp-overlay.dtbo \
		display/artl-sde-display-qrd-sku1-overlay.dtbo \
		display/artl-sde-display-qrd-sku2-overlay.dtbo \
		display/artl-sde-display-rcm-overlay.dtbo \
		display/arth-sde.dtbo \
		display/arth-sde-display-cdp-overlay.dtbo \
		display/arth-sde-display-mtp-overlay.dtbo
else
dtbo-$(CONFIG_ARCH_ART) += display/trustedvm-art-sde-display-omtp-overlay.dtbo \
			display/trustedvm-art-sde-display-cdp-overlay.dtbo \
			display/trustedvm-art-sde-display-qrd-overlay.dtbo \
			display/trustedvm-art-sde-display-mtp-overlay.dtbo
endif

ifneq ($(CONFIG_ARCH_QTI_VM), y)
dtbo-$(CONFIG_ARCH_CANOE) += display/canoe-sde.dtbo \
		display/canoe-sde-display-rumi-overlay.dtbo\
		display/canoe-sde-display-cdp-overlay.dtbo \
		display/canoe-sde-display-mtp-overlay.dtbo \
		display/canoe-sde-display-atp-overlay.dtbo \
		display/canoe-sde-display-rcm-overlay.dtbo \
		display/canoe-sde-display-cdp-kiwi-overlay.dtbo \
		display/canoe-sde-display-rcm-kiwi-overlay.dtbo \
		display/canoe-sde-display-cdp-st54l-pandeiro-overlay.dtbo \
		display/canoe-sde-display-rcm-st54l-pandeiro-overlay.dtbo \
		display/canoe-sde-display-qrd-sku1-overlay.dtbo \
		display/canoe-sde-display-qrd-sku2-overlay.dtbo \
		display/canoe-sde-display-hdk-overlay.dtbo \
		display/alor-interposer-sde-display-mtp-overlay.dtbo \
		display/alor-interposer-sde-display-rcm-overlay.dtbo \
		display/alor-interposer-sde-display-qrd-overlay.dtbo \
		display/alor-interposer-sde.dtbo
else
dtbo-$(CONFIG_ARCH_CANOE) += display/trustedvm-canoe-sde-display-mtp-overlay.dtbo \
# 		display/trustedvm-canoe-sde-display-cdp-overlay.dtbo \
# 		display/trustedvm-canoe-sde-display-qrd-overlay.dtbo \
# 		display/trustedvm-alor-interposer-sde-display-mtp-overlay.dtbo \
# 		display/trustedvm-alor-interposer-sde-display-rcm-overlay.dtbo \
# 		display/trustedvm-alor-interposer-sde-display-qrd-overlay.dtbo
endif

ifneq ($(CONFIG_ARCH_QTI_VM), y)
dtbo-$(CONFIG_ARCH_ALOR) += display/alor-sde.dtbo \
		display/alor-sde-display-atp-overlay.dtbo \
		display/alor-sde-display-cdp-overlay.dtbo \
		display/alor-sde-display-mtp-overlay.dtbo \
		display/alor-sde-display-qrd-overlay.dtbo \
		display/alor-sde-display-rcm-overlay.dtbo \
		display/alor-sde-display-rumi-overlay.dtbo \
		display/alor-sde-display-mtp-pmih010x-smb1398-overlay.dtbo \
		display/alor-sde-display-mtp-harmonium-overlay.dtbo
else
dtbo-$(CONFIG_ARCH_ALOR) += display/trustedvm-alor-sde-display-atp-overlay.dtbo \
		display/trustedvm-alor-sde-display-cdp-overlay.dtbo \
		display/trustedvm-alor-sde-display-mtp-overlay.dtbo \
		display/trustedvm-alor-sde-display-qrd-overlay.dtbo \
		display/trustedvm-alor-sde-display-rcm-overlay.dtbo
endif

ifneq ($(CONFIG_ARCH_QTI_VM), y)
dtbo-$(CONFIG_ARCH_PEBBLE) += display/pebble-sde.dtbo \
		display/pebble-sde-display-cdp-overlay.dtbo \
		display/pebble-sde-display-mtp-overlay.dtbo \
		display/pebble-sde-display-qrd-overlay.dtbo \
		display/pebble-sde-display-rcm-overlay.dtbo \
		display/pebble-sde-display-atp-overlay.dtbo
else
dtbo-$(CONFIG_ARCH_PEBBLE) += display/trustedvm-pebble-sde-display-mtp-overlay.dtbo \
		display/trustedvm-pebble-sde-display-cdp-overlay.dtbo \
		display/trustedvm-pebble-sde-display-qrd-overlay.dtbo \
		display/trustedvm-pebble-sde-display-rcm-overlay.dtbo \
		display/trustedvm-pebble-sde-display-atp-overlay.dtbo
endif

ifneq ($(CONFIG_ARCH_QTI_VM), y)
dtbo-$(CONFIG_ARCH_X1P42100) += display/x1p42100-sde.dtbo \
		display/x1p42100-sde-display-crd-overlay.dtbo
endif

ifneq ($(CONFIG_ARCH_QTI_VM), y)
dtbo-$(CONFIG_ARCH_X1E80100) += display/x1e80100-sde.dtbo \
		display/x1e80100-sde-display-crd-overlay.dtbo \
		display/x1e80100-sde-display-qcb-overlay.dtbo \
		display/x1e80100-sde-display-qcp-overlay.dtbo
endif

ifneq ($(CONFIG_ARCH_QTI_VM), y)
dtbo-$(CONFIG_ARCH_SUN) += display/sun-sde.dtbo \
		display/sun-sde-display-cdp-overlay.dtbo \
		display/sun-sde-display-mtp-overlay.dtbo \
		display/sun-sde-display-rumi-overlay.dtbo \
		display/sun-sde-display-rcm-overlay.dtbo \
		display/sun-sde-display-qrd-sku1-overlay.dtbo \
		display/sun-sde-display-qrd-sku1-v8-overlay.dtbo \
		display/sun-sde-display-qrd-sku2-v8-overlay.dtbo \
		display/sun-sde-display-cdp-kiwi-overlay.dtbo \
		display/sun-sde-display-mtp-kiwi-overlay.dtbo \
		display/sun-sde-display-cdp-kiwi-v8-overlay.dtbo \
		display/sun-sde-display-mtp-kiwi-v8-overlay.dtbo \
		display/sun-sde-display-cdp-nfc-overlay.dtbo \
		display/sun-sde-display-mtp-nfc-overlay.dtbo \
		display/sun-sde-display-cdp-v8-overlay.dtbo \
		display/sun-sde-display-mtp-v8-overlay.dtbo \
		display/sun-sde-display-atp-overlay.dtbo \
		display/sun-sde-display-mtp-3-5mm-overlay.dtbo \
		display/sun-sde-display-rcm-kiwi-overlay.dtbo \
		display/sun-sde-display-rcm-kiwi-v8-overlay.dtbo \
		display/sun-sde-display-rcm-v8-overlay.dtbo \
		display/sun-sde-display-mtp-qmp1000-overlay.dtbo \
		display/sun-sde-display-mtp-qmp1000-v8-overlay.dtbo \
		display/sun-sde-display-hdk-overlay.dtbo \
		display/sun-sde-display-hdk-sec-hdmi-overlay.dtbo \
		display/sun-sde-display-cdp-no-display-overlay.dtbo
else
dtbo-$(CONFIG_ARCH_SUN) += display/trustedvm-sun-sde-display-cdp-overlay.dtbo \
		display/trustedvm-sun-sde-display-mtp-overlay.dtbo \
		display/trustedvm-sun-sde-display-qrd-overlay.dtbo
endif

ifneq ($(CONFIG_ARCH_QTI_VM), y)
dtbo-$(CONFIG_ARCH_SHIKRA) += display/shikra-sde.dtbo \
		display/shikra-sde-display-itp-overlay.dtbo \
		display/shikra-sde-display-eitp-overlay.dtbo \
		display/shikra-sde-display-atp-overlay.dtbo \
		display/shikra-sde-display-itps-overlay.dtbo
else
dtbo-$(CONFIG_ARCH_SHIKRA) += display/trustedvm-shikra-sde-display-itps-eitp-overlay.dtbo
endif

ifneq ($(CONFIG_ARCH_QTI_VM), y)
dtbo-$(CONFIG_ARCH_PARROT) += display/parrot-sde.dtbo \
		display/parrot-sde-display-atp-overlay.dtbo \
		display/parrot-sde-display-idp-overlay.dtbo \
		display/parrot-sde-display-idp-amoled-overlay.dtbo \
		display/parrot-sde-display-rumi-overlay.dtbo \
		display/parrot-sde-display-qrd-overlay.dtbo
else
dtbo-$(CONFIG_ARCH_PARROT) += display/trustedvm-parrot-sde-display-idp-overlay.dtbo
endif

ifneq ($(CONFIG_ARCH_QTI_VM), y)
dtbo-$(CONFIG_ARCH_RAVELIN) += display/ravelin-sde.dtbo \
		display/ravelin-sde-display-atp-overlay.dtbo \
		display/ravelin-sde-display-idp-overlay.dtbo \
		display/ravelin-sde-display-idp-amoled-overlay.dtbo \
		display/ravelin-sde-display-rumi-overlay.dtbo \
		display/ravelin-sde-display-qrd-overlay.dtbo
else
dtbo-$(CONFIG_ARCH_RAVELIN) += display/trustedvm-ravelin-sde-display-idp-overlay.dtbo \
			display/trustedvm-ravelin-sde-display-idp-amoled-overlay.dtbo
endif

ifneq ($(CONFIG_ARCH_QTI_VM), y)
dtbo-$(CONFIG_ARCH_BOURTZI) += display/bourtzi-sde.dtbo \
		display/bourtzi-sde-display-mtp-amoled-overlay.dtbo \
		display/bourtzi-sde-display-cdp-amoled-overlay.dtbo \
		display/bourtzi-sde-display-atp-amoled-overlay.dtbo \
		display/bourtzi-sde-display-rcm-amoled-overlay.dtbo \
		display/bourtzi-sde-display-qrd-amoled-overlay.dtbo
else
dtbo-$(CONFIG_ARCH_BOURTZI) += display/trustedvm-bourtzi-sde-display-mtp-amoled-overlay.dtbo
endif

ifneq ($(CONFIG_ARCH_QTI_VM), y)
dtbo-$(CONFIG_ARCH_TUNA) += display/tuna-sde.dtbo \
		display/tuna-sde-display-atp-overlay.dtbo \
		display/tuna-sde-display-cdp-overlay.dtbo \
		display/tuna-sde-display-mtp-overlay.dtbo \
		display/tuna-sde-display-mtp-kiwi-harmonium-overlay.dtbo \
		display/tuna-sde-display-qrd-overlay.dtbo \
		display/tuna-sde-display-rumi-overlay.dtbo \
		display/tuna-sde-display-rcm-overlay.dtbo
else
dtbo-$(CONFIG_ARCH_TUNA) += display/trustedvm-tuna-sde-display-atp-overlay.dtbo \
		display/trustedvm-tuna-sde-display-cdp-overlay.dtbo \
		display/trustedvm-tuna-sde-display-mtp-overlay.dtbo \
		display/trustedvm-tuna-sde-display-mtp-kiwi-harmonium-overlay.dtbo \
		display/trustedvm-tuna-sde-display-qrd-overlay.dtbo \
		display/trustedvm-tuna-sde-display-rumi-overlay.dtbo \
		display/trustedvm-tuna-sde-display-rcm-overlay.dtbo
endif

ifneq ($(CONFIG_ARCH_QTI_VM), y)
dtbo-$(CONFIG_ARCH_KERA) += display/kera-sde.dtbo \
		display/kera-sde-display-atp-overlay.dtbo \
		display/kera-sde-display-cdp-overlay.dtbo \
		display/kera-sde-display-idp-no-display-overlay.dtbo \
		display/kera-sde-display-idp-overlay.dtbo \
		display/kera-sde-display-mtp-overlay.dtbo \
		display/kera-sde-display-qrd-overlay.dtbo \
		display/kera-sde-display-rumi-overlay.dtbo \
		display/kera-sde-display-rcm-overlay.dtbo \
		display/kera-sde-display-iot-cdp-overlay.dtbo \
		display/kera-sde-display-rcm-no-display-overlay.dtbo \
		display/kera-sde-display-iot-evk-overlay.dtbo \
		display/kera-sde-display-iot-eitp-overlay.dtbo
else
dtbo-$(CONFIG_ARCH_KERA) += display/trustedvm-kera-sde-display-atp-overlay.dtbo \
		display/trustedvm-kera-sde-display-cdp-overlay.dtbo \
		display/trustedvm-kera-sde-display-mtp-overlay.dtbo \
		display/trustedvm-kera-sde-display-qrd-overlay.dtbo \
		display/trustedvm-kera-sde-display-rumi-overlay.dtbo \
		display/trustedvm-kera-sde-display-rcm-overlay.dtbo
endif

ifneq ($(CONFIG_ARCH_QTI_VM), y)
dtbo-$(CONFIG_ARCH_MALABAR) += display/malabar-sde.dtbo \
		display/malabar-sde-display-atp-overlay.dtbo \
		display/malabar-sde-display-cdp-overlay.dtbo \
		display/malabar-sde-display-mtp-overlay.dtbo \
		display/malabar-sde-display-mtp-cottid-nd-overlay.dtbo \
		display/malabar-sde-display-qrd-overlay.dtbo \
		display/malabar-sde-display-rcm-overlay.dtbo
else
dtbo-$(CONFIG_ARCH_MALABAR) += display/trustedvm-malabar-sde-display-atp-overlay.dtbo \
		display/trustedvm-malabar-sde-display-cdp-overlay.dtbo
endif

ifeq ($(strip $(MSM_ARCH)),vienna_le)
dtbo-$(CONFIG_ARCH_VIENNA) += display/vienna-sde.dtbo \
		display/vienna_le-sde-no-display-overlay.dtbo
else
dtbo-$(CONFIG_ARCH_VIENNA) += display/vienna-sde.dtbo \
		display/vienna-sde-display-wdp-overlay.dtbo \
		display/vienna-sde-display-idp-overlay.dtbo \
		display/vienna-sde-display-wrd-overlay.dtbo \
		display/vienna-sde-display-atp-overlay.dtbo \
		display/vienna-sde-display-rcm-overlay.dtbo
endif

dtbo-$(CONFIG_ARCH_MONACO) += display/monaco-sde.dtbo \
		display/monaco-sde-display-idp-overlay.dtbo \
		display/monaco-sde-display-wdp-overlay.dtbo

dtbo-$(CONFIG_ARCH_KHAJE) += display/khaje-sde.dtbo \
		display/khaje-sde-display-idp-overlay.dtbo \
		display/khaje-sde-display-qrd-overlay.dtbo \
		display/khaje-sde-display-qrd-hvdcp3p5-overlay.dtbo \
		display/khaje-sde-display-idps-90hz-overlay.dtbo \
		display/khaje-sde-display-atp-overlay.dtbo \
		display/khaje-sde-display-idp-nopmi-overlay.dtbo \
		display/khaje-sde-display-qrd-nopmi-overlay.dtbo \
		display/khaje-sde-display-qrd-nowcd9375-overlay.dtbo

dtbo-$(CONFIG_ARCH_SCUBA) += display/scuba-iot-sde.dtbo \
                display/scuba-iot-sde-display-idp-overlay.dtbo

ifneq ($(CONFIG_ARCH_QTI_VM), y)
		CONFIG_OS_DTS := false
		ifeq ($(shell [[ $(VERSION) -eq 6 && $(PATCHLEVEL) -ge 6 ]] && echo true), true)
			CONFIG_OS_DTS := true
		endif

		ifeq ($(CONFIG_OS_DTS), true)
			dtbo-$(CONFIG_ARCH_SERAPH) += display/seraph-sde.dtbo \
			display/seraph-sde-display-idp-overlay.dtbo \
			display/seraph-sde-display-idp-no-display-overlay.dtbo \
			display/seraph-sde-display-rumi-overlay.dtbo \
			display/seraphp-sde-display-qar-overlay.dtbo
		endif
endif

dtbo-$(CONFIG_ARCH_BENGAL) += display/bengal-sde.dtbo \
		display/bengal-sde-display-idp-overlay.dtbo \
		display/bengal-sde-display-qrd-overlay.dtbo \
		display/bengal-sde-display-idp-nopmi-overlay.dtbo \
		display/bengal-sde-display-rb2-v4-overlay.dtbo

ifneq ($(CONFIG_ARCH_QTI_VM), y)
dtbo-$(CONFIG_ARCH_PIKACHU) += display/pikachu-sde.dtbo \
		display/pikachu-sde-display-atp-overlay.dtbo \
		display/pikachu-sde-display-idp-overlay.dtbo \
		display/pikachu-sde-display-rumi-overlay.dtbo
endif

always-y    := $(dtb-y) $(dtbo-y)
subdir-y    := $(dts-dirs)
clean-files    := *.dtb *.dtbo
