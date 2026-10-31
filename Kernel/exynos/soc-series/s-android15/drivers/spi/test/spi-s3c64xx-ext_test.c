// SPDX-License-Identifier: GPL-2.0-only
/**
 * spi-s3c64xx-ext_test.c - Samsung Exynos5 I2C Controller Test Driver for KUNIT
 *
 * Copyright (C) 2024 Samsung Electronics Co., Ltd.
 */

#include <kunit/test.h>
#include <soc/samsung/spi-s3c64xx.h>
#include <kunit/visibility.h>

unsigned int s3c64xx_spi_get_max_transfer_packet(struct s3c64xx_spi_driver_data *sdd);

MODULE_IMPORT_NS(EXPORTED_FOR_KUNIT_TESTING);

/* SDD Driver for USIv2 */
static struct s3c64xx_spi_driver_data sdd_v2;
static struct s3c64xx_spi_info sci_v2;

/* SDD Driver for USIv3 */
static struct s3c64xx_spi_driver_data sdd_v3;
static struct s3c64xx_spi_info sci_v3;

static int spi_s3c64xx_ext_test_init(struct kunit *test)
{
	/* Init USIv2 SPI Driver Data */
	memset(&sdd_v2, 0, sizeof(sdd_v2));
	memset(&sci_v2, 0, sizeof(sci_v2));

	sci_v2.usi_version = 2;
	sdd_v2.cntrlr_info = &sci_v2;
	
	/* Init USIv2 SPI Driver Data */
	memset(&sdd_v3, 0, sizeof(sdd_v3));
	memset(&sci_v3, 0, sizeof(sci_v3));

	sci_v3.usi_version = 3;
	sdd_v3.cntrlr_info = &sci_v3;

	return 0;
}

static void spi_s3c64xx_ext_sample_test(struct kunit *test)
{
	int result = 0;

	result = s3c64xx_spi_get_max_transfer_packet(&sdd_v2);
	KUNIT_EXPECT_EQ(test, 0xfff0, result);

	result = s3c64xx_spi_get_max_transfer_packet(&sdd_v3);
	KUNIT_EXPECT_EQ(test, 0xFFFFEF, result);
}

static struct kunit_case spi_s3c64xx_ext_test_cases[] = {
	KUNIT_CASE(spi_s3c64xx_ext_sample_test),
	{},
};

static struct kunit_suite spi_s3c64xx_ext_test_suite = {
	.name = "spi_exynos",
	.init = spi_s3c64xx_ext_test_init,
	.test_cases = spi_s3c64xx_ext_test_cases,
};

kunit_test_suites(&spi_s3c64xx_ext_test_suite);

MODULE_LICENSE("GPL v2");
