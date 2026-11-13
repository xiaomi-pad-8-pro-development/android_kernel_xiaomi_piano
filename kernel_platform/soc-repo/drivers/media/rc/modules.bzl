def register_modules(registry):
    registry.register(
        name = "drivers/media/rc/ir-spi",
        out = "ir-spi.ko",
        config = "CONFIG_IR_SPI",
        srcs = [
            # do not sort
            "drivers/media/rc/ir-spi.c",
        ],
    )
