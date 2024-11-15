// SPDX-License-Identifier: GPL-2.0-or-later

/*************************************************************************
 * Support for STM32H5xx devices.
 *
 * (C) 2024, SanSys Electronic GmbH
 * info@sansys-electronic.com
 *
 *************************************************************************/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdint.h>

#include "imp.h"
#include "target/arm.h"
#include <helper/log.h>
//#include <helper/align.h>
//#include <helper/binarybuffer.h>
//#include <helper/bits.h>
//#include <target/algorithm.h>
//#include <target/arm_adi_v5.h>
//#include <target/cortex_m.h>

struct RevIDList_s {
	uint32_t						RevID;
	char							Revision;
};

struct STM32H5xxDef_s {
	/* Device definition. */
	enum arm_arch					ARMArch;
	uint32_t						IDCODE_RomTableAddr;
	uint32_t						DevID;
	const char*						DevStr;
	const struct RevIDList_s*		RevIDList;
	uint32_t						MaxFlashSize;
	uint32_t						FlashSize_Addr;
	int								(* MassErase)(struct flash_bank*);
	int								(* Erase)(struct flash_bank*, unsigned int, unsigned int);
};

struct STM32H5xxPrv_s {
	const struct STM32H5xxDef_s*	Dev;
	uint32_t						FlashSize;
};



/*****************************************************************************************
 * STM32U5 specific
 ****************************************************************************************/
#define	U5_NSKEYR				(0x40022000 + 0x008)
#define	U5_NSSR					(0x40022000 + 0x020)
#define	U5_NSCR					(0x40022000 + 0x028)

static int Lock_U5(struct flash_bank* bank) {
	return target_write_u32(bank->target, U5_NSCR, 0x80000000);
}
static int Unlock_U5(struct flash_bank* bank) {
	int ret								= ERROR_FAIL;
	uint32_t val;
	/* Check if locked. */
	if ((ret = target_read_u32(bank->target, U5_NSCR, &val)) == ERROR_OK) {
		if ((val & 0x80000000) == 0) {
			ret			= ERROR_OK;		/* Still unlocked. */
		}
		else if ((ret = target_write_u32(bank->target, U5_NSKEYR, 0x45670123)) == ERROR_OK
				&& (ret = target_write_u32(bank->target, U5_NSKEYR, 0xcdef89ab)) == ERROR_OK) {
			if ((ret = target_read_u32(bank->target, U5_NSCR, &val)) == ERROR_OK) {
				if ((val & 0x80000000) == 0) {
					ret			= ERROR_OK;		/* Now unlocked. */
				}
				else {
					ret			= ERROR_FAIL;	/* Locked until system reset. */
				}
			}
		}
	}
	return ret;
}

static int MassErase_U5(struct flash_bank* bank) {
	int ret								= ERROR_FAIL;
	uint32_t val;
	/* Check if no operation is on-going. */
	if ((ret = target_read_u32(bank->target, U5_NSSR, &val)) == ERROR_OK) {
		if ((val & 0x00010000) == 0) {
			/* Clear all error flags. */
			if ((ret = target_write_u32(bank->target, U5_NSSR, 0x000020fb)) == ERROR_OK) {
				/* Unlock flash */
				if ((ret = Unlock_U5(bank)) == ERROR_OK) {
					/* Start mass erase */
					if ((ret = target_write_u32(bank->target, U5_NSCR, 0x00018004)) == ERROR_OK) {
						uint32_t timeout		= 3000;
						while (0 < timeout) {
							alive_sleep(1);
							if ((ret = target_read_u32(bank->target, U5_NSSR, &val)) == ERROR_OK) {
								if ((val & 0x000020fa) != 0) {
									ret		= ERROR_FAIL;
									break;
								}
								else if ((val & 0x00010001) == 0x00000001) {
									ret		= ERROR_OK;
									break;
								}
							}
							timeout--;
						}

						/* Reset MER1 and MER2 bits */
						target_write_u32(bank->target, U5_NSCR, 0x00000000);
					}

					/* Lock flash */
					Lock_U5(bank);
				}
			}
		}
		else {
			ret		= ERROR_TARGET_RESOURCE_NOT_AVAILABLE;
		}
	}
	return ret;
}

static int Erase_U5(struct flash_bank* bank, unsigned int first, unsigned int last) {
	return 0;
}
#undef U5_NSKEYR
#undef U5_NSSR
#undef U5_NSCR

/*****************************************************************************************
 * STM32H5 specific
 ****************************************************************************************/
#define	H5_NSKEYR				(0x40022000 + 0x004)
#define	H5_NSSR					(0x40022000 + 0x020)
#define	H5_NSCR					(0x40022000 + 0x028)
#define	H5_NSCCR				(0x40022000 + 0x030)

static int Lock_H5(struct flash_bank* bank) {
	return target_write_u32(bank->target, H5_NSCR, 0x00000001);
}
static int Unlock_H5(struct flash_bank* bank) {
	int ret								= ERROR_FAIL;
	uint32_t val;
	/* Check if locked. */
	if ((ret = target_read_u32(bank->target, H5_NSCR, &val)) == ERROR_OK) {
		if ((val & 0x00000001) == 0) {
			ret			= ERROR_OK;		/* Still unlocked. */
		}
		else if ((ret = target_write_u32(bank->target, H5_NSKEYR, 0x45670123)) == ERROR_OK
				&& (ret = target_write_u32(bank->target, H5_NSKEYR, 0xcdef89ab)) == ERROR_OK) {
			if ((ret = target_read_u32(bank->target, H5_NSCR, &val)) == ERROR_OK) {
				if ((val & 0x00000001) == 0) {
					ret			= ERROR_OK;		/* Now unlocked. */
				}
				else {
					ret			= ERROR_FAIL;	/* Locked until system reset. */
				}
			}
		}
	}
	return ret;
}

static int MassErase_H5(struct flash_bank* bank) {
	int ret								= ERROR_FAIL;
	uint32_t val;
	/* Check if no operation is on-going. */
	if ((ret = target_read_u32(bank->target, H5_NSSR, &val)) == ERROR_OK) {
		if ((val & 0x0000000b) == 0) {
			/* Clear all error flags. */
			if ((ret = target_write_u32(bank->target, H5_NSCCR, 0x00ff0000)) == ERROR_OK) {
				/* Unlock flash */
				if ((ret = Unlock_H5(bank)) == ERROR_OK) {
					/* Start mass erase */
					if ((ret = target_write_u32(bank->target, H5_NSCR, 0x00008020)) == ERROR_OK) {
						uint32_t timeout		= 3000;
						while (0 < timeout) {
							alive_sleep(1);
							if ((ret = target_read_u32(bank->target, H5_NSSR, &val)) == ERROR_OK) {
								if ((val & 0x00fe0000) != 0) {
									ret		= ERROR_FAIL;
									break;
								}
								else if ((val & 0x0001000b) == 0x00010000) {
									ret		= ERROR_OK;
									break;
								}
							}
							timeout--;
						}

						/* Reset MER bit */
						target_write_u32(bank->target, H5_NSCR, 0x00008000);
					}

					/* Lock flash */
					Lock_H5(bank);
				}
			}
		}
		else {
			ret		= ERROR_TARGET_RESOURCE_NOT_AVAILABLE;
		}
	}
	return ret;
}

static int Erase_H5(struct flash_bank* bank, unsigned int first, unsigned int last) {
	return 0;
}
#undef H5_NSKEYR
#undef H5_NSSR
#undef H5_NSCR
#undef H5_NSCCR

/*****************************************************************************************
 * STM32H7 specific
 ****************************************************************************************/
#define	H7_KEYR(_bank_)			(0x52002000 + ((_bank_) == 0 ? 0x004 : 0x104))
#define	H7_SR(_bank_)			(0x52002000 + ((_bank_) == 0 ? 0x010 : 0x110))
#define	H7_CR(_bank_)			(0x52002000 + ((_bank_) == 0 ? 0x00c : 0x10c))
#define	H7_CCR(_bank_)			(0x52002000 + ((_bank_) == 0 ? 0x014 : 0x114))

static int Lock_H7(struct flash_bank* bank, int bankNum) {
	return target_write_u32(bank->target, H7_CR(bankNum), 0x00000001);
}
static int Unlock_H7(struct flash_bank* bank, int bankNum) {
	int ret								= ERROR_FAIL;
	uint32_t val;
	/* Check if locked. */
	if ((ret = target_read_u32(bank->target, H7_CR(bankNum), &val)) == ERROR_OK) {
		if ((val & 0x00000001) == 0) {
			ret			= ERROR_OK;		/* Still unlocked. */
		}
		else if ((ret = target_write_u32(bank->target, H7_KEYR(bankNum), 0x45670123)) == ERROR_OK
				&& (ret = target_write_u32(bank->target, H7_KEYR(bankNum), 0xcdef89ab)) == ERROR_OK) {
			if ((ret = target_read_u32(bank->target, H7_CR(bankNum), &val)) == ERROR_OK) {
				if ((val & 0x00000001) == 0) {
					ret			= ERROR_OK;		/* Now unlocked. */
				}
				else {
					ret			= ERROR_FAIL;	/* Locked until system reset. */
				}
			}
		}
	}
	return ret;
}

static int MassErase_H7(struct flash_bank* bank) {
	int ret								= ERROR_FAIL;
	uint32_t val;
	for (int bankNum = 0; bankNum < 2; bankNum++) {
		/* Check if no operation is on-going. */
		if ((ret = target_read_u32(bank->target, H7_SR(bankNum), &val)) == ERROR_OK) {
			if ((val & 0x0000000b) == 0) {
				/* Clear all error flags. */
				if ((ret = target_write_u32(bank->target, H7_CCR(bankNum), 0x00ff0000)) == ERROR_OK) {
					/* Unlock flash */
					if ((ret = Unlock_H7(bank, bankNum)) == ERROR_OK) {
						/* Start mass erase */
						if ((ret = target_write_u32(bank->target, H7_CR(bankNum), 0x00008020)) == ERROR_OK) {
							uint32_t timeout		= 3000;
							while (0 < timeout) {
								alive_sleep(1);
								if ((ret = target_read_u32(bank->target, H7_SR(bankNum), &val)) == ERROR_OK) {
									if ((val & 0x00ff0000) != 0) {
										ret		= ERROR_FAIL;
										break;
									}
									else if ((val & 0x0000000b) == 0) {
										ret		= ERROR_OK;
										break;
									}
								}
								timeout--;
							}

							/* Reset MER bit */
							target_write_u32(bank->target, H7_CR(bankNum), 0x00008000);
						}

						/* Lock flash */
						Lock_H7(bank, bankNum);
					}
				}
			}
			else {
				ret		= ERROR_TARGET_RESOURCE_NOT_AVAILABLE;
			}
		}
	}
	return ret;
}

static int Erase_H7(struct flash_bank* bank, unsigned int first, unsigned int last) {
	return 0;
}
#undef H7_KEYR
#undef H7_SR
#undef H7_CR
#undef H7_CCR


const struct RevIDList_s RevIDList_U5F_U5G[] = {{0x1000, 'A'}, {0x1001, 'Z'}, {0, '\0'}};
const struct RevIDList_s RevIDList_U59_U5A[] = {{0x3001, 'X'}, {0, '\0'}};
const struct RevIDList_s RevIDList_U575_U585[] = {{0x2001, 'X'}, {0x3001, 'W'}, {0, '\0'}};
const struct RevIDList_s RevIDList_U535_U545[] = {{0x1001, 'Z'}, {0, '\0'}};
const struct RevIDList_s RevIDList_H5[] = {{0x1000, 'A'}, {0x1001, 'Z'}, {0x1007, 'X'}, {0, '\0'}};
const struct RevIDList_s RevIDList_H7[] = {{0x1001, 'Z'}, {0x1003, 'Y'}, {0x2001, 'X'}, {0x2003, 'V'}, {0, '\0'}};
const struct STM32H5xxDef_s DeviceDefs[] = {
	/* U535xB	128k
	 * U535xC	256k
	 * U535xE	512k
	 * U545xE	512k */
	{	.ARMArch					= ARM_ARCH_V8M,
		.IDCODE_RomTableAddr		= 0xe0044000,			/* RM0456, 75.5, ROM Tables */
		.DevID						= 0x455,				/* RM0456, 75.12.4, DBGMCU_IDCODE */
		.DevStr						= "STM32U535/545",
		.RevIDList					= RevIDList_U535_U545,
		.MaxFlashSize				= 512*1024,				/* DS, 7 Ordering information */
		.FlashSize_Addr				= 0x0bfa07a0,			/* RM0456, 76.2 Flash size data register */
		.MassErase					= MassErase_U5,
		.Erase						= Erase_U5,
	},
	/* U5GxxJ	4M
	 * U5FxxJ	4M
	 * U5FxxI	2M */
	{	.ARMArch					= ARM_ARCH_V8M,
		.IDCODE_RomTableAddr		= 0xe0044000,			/* RM0456, 75.5, ROM Tables */
		.DevID						= 0x476,				/* RM0456, 75.12.4, DBGMCU_IDCODE */
		.DevStr						= "STM32U5Fx/5Gx",
		.RevIDList					= RevIDList_U5F_U5G,
		.MaxFlashSize				= 4*1024*1024,			/* DS, 7 Ordering information */
		.FlashSize_Addr				= 0x0bfa07a0,			/* RM0456, 76.2 Flash size data register */
		.MassErase					= MassErase_U5,
		.Erase						= Erase_U5,
	},
	/* U5AxxI	2M
	 * U5AxxJ	4M
	 * U59xxI	2M
	 * U59xxJ	4M */
	{	.ARMArch					= ARM_ARCH_V8M,
		.IDCODE_RomTableAddr		= 0xe0044000,			/* RM0456, 75.5, ROM Tables */
		.DevID						= 0x481,				/* RM0456, 75.12.4, DBGMCU_IDCODE */
		.DevStr						= "STM32U59x/5Ax",
		.RevIDList					= RevIDList_U59_U5A,
		.MaxFlashSize				= 4*1024*1024,			/* DS, 7 Ordering information */
		.FlashSize_Addr				= 0x0bfa07a0,			/* RM0456, 76.2 Flash size data register */
		.MassErase					= MassErase_U5,
		.Erase						= Erase_U5,
	},
	/* U585xI	2M
	 * U575xG	1M
	 * U575xI	2M */
	{	.ARMArch					= ARM_ARCH_V8M,
		.IDCODE_RomTableAddr		= 0xe0044000,			/* RM0456, 75.5, ROM Tables */
		.DevID						= 0x482,				/* RM0456, 75.12.4, DBGMCU_IDCODE */
		.DevStr						= "STM32U575/585",
		.RevIDList					= RevIDList_U575_U585,
		.MaxFlashSize				= 2*1024*1024,			/* DS, 7 Ordering information */
		.FlashSize_Addr				= 0x0bfa07a0,			/* RM0456, 76.2 Flash size data register */
		.MassErase					= MassErase_U5,
		.Erase						= Erase_U5,
	},
	/* H56xxG	1M
	 * H56xxI	2M
	 * H573xI	2M
	 */
	{	.ARMArch					= ARM_ARCH_V8M,
		.IDCODE_RomTableAddr		= 0x44024000,			/* RM0481, 59.5, ROM Tables */
		.DevID						= 0x484,				/* RM0481, 59.12.4, DBGMCU_IDCODE */
		.DevStr						= "STM32H562/563/573",
		.RevIDList					= RevIDList_H5,
		.MaxFlashSize				= 2*1024*1024,			/* DS, 7 Ordering information */
		.FlashSize_Addr				= 0x08fff80c,			/* RM0481, 60.2 Flash size data register */
		.MassErase					= MassErase_H5,
		.Erase						= Erase_H5,
	},
	/* H523xC	256k
	 * H523xE	512k
	 * H533xE	512k */
	{	.ARMArch					= ARM_ARCH_V8M,
		.IDCODE_RomTableAddr		= 0x44024000,			/* RM0481, 59.5, ROM Tables */
		.DevID						= 0x478,				/* RM0481, 59.12.4, DBGMCU_IDCODE */
		.DevStr						= "STM32H523/533",
		.RevIDList					= RevIDList_H5,
		.MaxFlashSize				= 512*1024,				/* DS, 7 Ordering information */
		.FlashSize_Addr				= 0x08fff80c,			/* RM0481, 60.2 Flash size data register */
		.MassErase					= MassErase_H5,
		.Erase						= Erase_H5,
	},
	/* H503xB	128k */
	{	.ARMArch					= ARM_ARCH_V8M,
		.IDCODE_RomTableAddr		= 0x44024000,			/* RM0492, 41.5, ROM Tables */
		.DevID						= 0x474,				/* RM0492, 41.12.4, DBGMCU_IDCODE */
		.DevStr						= "STM32H503",
		.RevIDList					= RevIDList_H5,
		.MaxFlashSize				= 512*1024,				/* DS, 7 Ordering information */
		.FlashSize_Addr				= 0x08fff80c,			/* RM0481, 60.2 Flash size data register */
		.MassErase					= MassErase_H5,
		.Erase						= Erase_H5,
	},
	/* H743xG	1M
	 * H743xI	2M
	 * H742xG	1M
	 * H742xI	2M
	 * H750xB	128k
	 * H753xG	1M */
	{	.ARMArch					= ARM_ARCH_V7M,
		.IDCODE_RomTableAddr		= 0x5c001000,			/* RM0492, 60.5.8, DBGMCU */
		.DevID						= 0x450,				/* RM0433, 60.5.8, DBGMCU_IDC */
		.DevStr						= "STM32H742/743/750/753",
		.RevIDList					= RevIDList_H7,
		.MaxFlashSize				= 2*1024*1024,			/* DS, 7 Ordering information */
		.FlashSize_Addr				= 0x1ff1e880,			/* RM0433, 61.2 Flash size */
		.MassErase					= MassErase_H7,
		.Erase						= Erase_H7,
	},
};

static int FlashMassErase(struct flash_bank* bank) {
	struct STM32H5xxPrv_s* this			= bank->driver_priv;
	const struct STM32H5xxDef_s* dev	= this->Dev;
	return (dev->MassErase)(bank);
}

static int FlashErase(struct flash_bank* bank, unsigned int first, unsigned int last) {
	struct STM32H5xxPrv_s* this			= bank->driver_priv;
	const struct STM32H5xxDef_s* dev	= this->Dev;
	return (dev->Erase)(bank, first, last);
}

static int GetInfo(struct flash_bank *bank, struct command_invocation *cmd) {
	struct STM32H5xxPrv_s* this			= bank->driver_priv;
	const struct STM32H5xxDef_s* dev	= this->Dev;
	if (dev != NULL) {
		command_print_sameline(cmd, "-");
	}
	return ERROR_OK;
}

static int Probe(struct flash_bank *bank)
{
	struct target *target = bank->target;
	struct STM32H5xxPrv_s* this			= bank->driver_priv;
	int ret								= ERROR_FAIL;

	if (target_was_examined(target)) {
		if (this->Dev == NULL) {
			struct arm* armTarget	= target_to_arm(target);
			if (armTarget != NULL && is_arm(armTarget)) {
				for (size_t k = 0; k < (sizeof(DeviceDefs)/sizeof(DeviceDefs[0])); k++) {
					if (armTarget->arch == DeviceDefs[k].ARMArch) {
						uint32_t idcode;

						if (target_read_u32(target, DeviceDefs[k].IDCODE_RomTableAddr, &idcode) == ERROR_OK) {
							if (DeviceDefs[k].DevID == (idcode & 0x0fff)) {
								LOG_INFO("%s found.", DeviceDefs[k].DevStr);
								this->Dev			= &(DeviceDefs[k]);
								
								/* Try to examine real flash size. */
								this->FlashSize		= this->Dev->MaxFlashSize;
								uint16_t val;
								uint32_t flashSize;
								if (target_read_u16(target, this->Dev->FlashSize_Addr, &val) == ERROR_OK) {
									flashSize		= ((uint32_t)val) * 1024;
									if (0 < flashSize && flashSize <= this->FlashSize) {
										this->FlashSize		= flashSize;
										LOG_INFO("Flash size: %ukB", flashSize/1024);
									}
									else {
										LOG_WARNING("MCU indicates invalid flash size (%ukB). Assuming default: %ukB", flashSize/1024, this->FlashSize/1024);
									}
								}
								else {
									LOG_WARNING("Unable to read flash size from MCU. Assuming default: %ukB", this->FlashSize/1024);
								}

								return ERROR_OK;
							}
						}
					}
				}
			}
			else {
				LOG_ERROR("Not a ARM target");
				ret		= ERROR_FAIL;
			}
		}
		else {
			ret		= ERROR_OK;		/* Still probed. */
		}
	}
	else {
		LOG_ERROR("Target not examined yet");
		ret		= ERROR_TARGET_NOT_EXAMINED;
	}
	return ret;
}

/* flash bank stm32u5_h5_h7 <base> <size> 0 0 <target#> */
FLASH_BANK_COMMAND_HANDLER(stm32u5_h5_h7_flash_bank_command)
{
	if (CMD_ARGC < 6)
		return ERROR_COMMAND_SYNTAX_ERROR;

	struct STM32H5xxPrv_s* prv	= malloc(sizeof(struct STM32H5xxPrv_s));
	if (prv != NULL) {
		prv->Dev			= NULL;
		bank->driver_priv	= prv;
		return ERROR_OK;
	}
	return ERROR_FAIL;
}

COMMAND_HANDLER(STM32_MassEraseCommand) {
	int ret;
	if (CMD_ARGC == 1) {
		struct flash_bank* bank;
		if ((ret = CALL_COMMAND_HANDLER(flash_command_get_bank, 0, &bank)) == ERROR_OK) {
			if ((ret = FlashMassErase(bank)) == ERROR_OK) {
				command_print(CMD, "Flash erased.");
			}
			else {
				command_print(CMD, "Flash erase failed!");
			}
		}
	}
	else {
		ret		= ERROR_COMMAND_SYNTAX_ERROR;
	}
	return ret;
}

static const struct command_registration STM32_ExecCommandHandlers[] = {
	{
		.name = "mass_erase",
		.handler = STM32_MassEraseCommand,
		.mode = COMMAND_EXEC,
		.usage = "bank_id",
		.help = "Erase entire flash device.",
	},
	COMMAND_REGISTRATION_DONE
};

static const struct command_registration STM32_CommandHandlers[] = {
	{
		.name = "stm32u5",
		.mode = COMMAND_ANY,
		.help = "stm32u5 flash command group",
		.usage = "",
		.chain = STM32_ExecCommandHandlers,
	},
	{
		.name = "stm32h5",
		.mode = COMMAND_ANY,
		.help = "stm32h5 flash command group",
		.usage = "",
		.chain = STM32_ExecCommandHandlers,
	},
	{
		.name = "stm32h7",
		.mode = COMMAND_ANY,
		.help = "stm32h7 flash command group",
		.usage = "",
		.chain = STM32_ExecCommandHandlers,
	},
	COMMAND_REGISTRATION_DONE
};

const struct flash_driver stm32u5_h5_h7_flash = {
	.name					= "stm32u5_h5_h7",
	.commands				= STM32_CommandHandlers,
	.flash_bank_command		= stm32u5_h5_h7_flash_bank_command,
	.erase					= FlashErase,
	.protect				= NULL, //stm32l4_protect,
	.write					= NULL, //stm32l4_write,
	.read					= NULL, //default_flash_read,
	.probe					= Probe,
	.auto_probe				= Probe,
	.erase_check			= default_flash_blank_check,
	.protect_check			= NULL, //stm32l4_protect_check,
	.info					= GetInfo,
	.free_driver_priv		= default_flash_free_driver_priv,
};

/* vi: set tw=140 ts=4 sw=4 noexpandtab cindent: */
