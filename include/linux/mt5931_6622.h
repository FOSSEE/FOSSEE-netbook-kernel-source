

#ifndef __MT5931__H__
#define __MT5931__H__

struct mt5931_sdio_eint_platform_data {
	/* used for sdio eint. */
	int sdio_eint;
};

struct mtk_mt5931_platform_data {
	/* GPIO pin config */
	int pmu;
	int rst;
};

struct mtk_mt6622_platform_data {
	/* GPIO pin config */
	int pmu;
	int rst;

	/* used for bt eint. */
	int bt_eint;
};

#endif
