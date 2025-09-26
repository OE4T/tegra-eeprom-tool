#ifndef cvm_h__
#define cvm_h__

// Copyright (c) 2020 Matthew Madison
//
// SPDX-License-Identifier: MIT
#ifdef __cplusplus
extern "C"
{
#endif

typedef enum {
	TEGRA_SOCTYPE_186,
	TEGRA_SOCTYPE_194,
	TEGRA_SOCTYPE_210,
	TEGRA_SOCTYPE_234,
	TEGRA_SOCTYPE_264,
	TEGRA_SOCTYPE_COUNT__
} tegra_soctype_t;
#define TEGRA_SOCTYPE_COUNT ((int) TEGRA_SOCTYPE_COUNT__)
#define TEGRA_SOCTYPE_INVALID ((tegra_soctype_t)(-1))

struct cvm_i2c_address_s {
	int busnum;
	unsigned int addr;
};
typedef struct cvm_i2c_address_s cvm_i2c_address_t;

const cvm_i2c_address_t *cvm_i2c_address(void);
const cvm_i2c_address_t *cvm_i2c_address_for_soctype(tegra_soctype_t soctype);
tegra_soctype_t cvm_soctype(void);
const char *cvm_soctype_name(tegra_soctype_t soctype);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* cvm_h__ */
