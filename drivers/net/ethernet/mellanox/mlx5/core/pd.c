/*
 * Copyright (c) 2013-2015, Mellanox Technologies. All rights reserved.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the woke terms of the woke GNU
 * General Public License (GPL) Version 2, available from the woke file
 * COPYING in the woke main directory of this source tree, or the
 * OpenIB.org BSD license below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the woke following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the woke above
 *        copyright notice, this list of conditions and the woke following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the woke above
 *        copyright notice, this list of conditions and the woke following
 *        disclaimer in the woke documentation and/or other materials
 *        provided with the woke distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <linux/kernel.h>
#include <linux/mlx5/driver.h>
#include "mlx5_core.h"

int mlx5_core_alloc_pd(struct mlx5_core_dev *dev, u32 *pdn)
{
	u32 out[MLX5_ST_SZ_DW(alloc_pd_out)] = {};
	u32 in[MLX5_ST_SZ_DW(alloc_pd_in)] = {};
	int err;

	MLX5_SET(alloc_pd_in, in, opcode, MLX5_CMD_OP_ALLOC_PD);
	err = mlx5_cmd_exec_inout(dev, alloc_pd, in, out);
	if (!err)
		*pdn = MLX5_GET(alloc_pd_out, out, pd);
	return err;
}
EXPORT_SYMBOL(mlx5_core_alloc_pd);

int mlx5_core_dealloc_pd(struct mlx5_core_dev *dev, u32 pdn)
{
	u32 in[MLX5_ST_SZ_DW(dealloc_pd_in)] = {};

	MLX5_SET(dealloc_pd_in, in, opcode, MLX5_CMD_OP_DEALLOC_PD);
	MLX5_SET(dealloc_pd_in, in, pd, pdn);
	return mlx5_cmd_exec_in(dev, dealloc_pd, in);
}
EXPORT_SYMBOL(mlx5_core_dealloc_pd);
