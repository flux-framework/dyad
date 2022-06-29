/************************************************************\
 * Copyright 2021 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, COPYING)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\************************************************************/

#include "flist.hpp"

void set_flist (std::string base, flist_t& flist)
{
  flist.clear ();
  if (!base.empty() && (base.back () != '/')) {
      base += '/';
  }
  flist.push_back (std::make_pair(base + "first_file_1byte.bin", 1));
  flist.push_back (std::make_pair(base + "model_weights_macc_wae0_dec_fc0_bias_64x1.bin", 272));
  flist.push_back (std::make_pair(base + "model_weights_macc_wae0_dec_fc0_matrix_64x20.bin", 5136));
  flist.push_back (std::make_pair(base + "model_weights_macc_wae0_dec_fc1_bias_128x1.bin", 528));
  flist.push_back (std::make_pair(base + "model_weights_macc_wae0_dec_fc1_matrix_128x64.bin", 32784));
  flist.push_back (std::make_pair(base + "model_weights_macc_wae0_dec_fc2_bias_256x1.bin", 1040));
  flist.push_back (std::make_pair(base + "model_weights_macc_wae0_dec_fc2_matrix_256x128.bin", 131088));
  flist.push_back (std::make_pair(base + "model_weights_macc_wae0_disc0_fc0_bias_128x1.bin", 528));
  flist.push_back (std::make_pair(base + "model_weights_macc_wae0_disc0_fc0_matrix_128x16419.bin", 8406544));
  flist.push_back (std::make_pair(base + "model_weights_macc_wae0_disc0_fc1_bias_64x1.bin", 272));
  flist.push_back (std::make_pair(base + "model_weights_macc_wae0_disc0_fc1_matrix_64x128.bin", 32784));
  flist.push_back (std::make_pair(base + "model_weights_macc_wae0_disc0_fc2_bias_1x1.bin", 20));
  flist.push_back (std::make_pair(base + "model_weights_macc_wae0_disc0_fc2_matrix_1x64.bin", 272));
  flist.push_back (std::make_pair(base + "model_weights_macc_wae0_disc1_fc0_bias_128x1.bin", 528));
  flist.push_back (std::make_pair(base + "model_weights_macc_wae0_disc1_fc0_matrix_128x16419.bin", 8406544));
  flist.push_back (std::make_pair(base + "model_weights_macc_wae0_disc1_fc1_bias_64x1.bin", 272));
  flist.push_back (std::make_pair(base + "model_weights_macc_wae0_disc1_fc1_matrix_64x128.bin", 32784));
  flist.push_back (std::make_pair(base + "model_weights_macc_wae0_disc1_fc2_bias_1x1.bin", 20));
  flist.push_back (std::make_pair(base + "model_weights_macc_wae0_disc1_fc2_matrix_1x64.bin", 272));
  flist.push_back (std::make_pair(base + "model_weights_macc_wae0_enc_fc0_bias_32x1.bin", 144));
  flist.push_back (std::make_pair(base + "model_weights_macc_wae0_enc_fc0_matrix_32x16399.bin", 2099088));
  flist.push_back (std::make_pair(base + "model_weights_macc_wae0_enc_fc1_bias_256x1.bin", 1040));
  flist.push_back (std::make_pair(base + "model_weights_macc_wae0_enc_fc1_matrix_256x32.bin", 32784));
  flist.push_back (std::make_pair(base + "model_weights_macc_wae0_enc_fc2_bias_128x1.bin", 528));
  flist.push_back (std::make_pair(base + "model_weights_macc_wae0_enc_fc2_matrix_128x256.bin", 131088));
  flist.push_back (std::make_pair(base + "model_weights_macc_wae0enc_out_bias_20x1.bin", 96));
  flist.push_back (std::make_pair(base + "model_weights_macc_wae0enc_out_matrix_20x128.bin", 10256));
  flist.push_back (std::make_pair(base + "model_weights_macc_wae0pred_y_bias_16399x1.bin", 65612));
  flist.push_back (std::make_pair(base + "model_weights_macc_wae0pred_y_matrix_16399x256.bin", 16792592));
  flist.push_back (std::make_pair(base + "train_macc_wae0_dec_fc0_bias_optimizer_adam_moment1_64x1.bin", 272));
  flist.push_back (std::make_pair(base + "train_macc_wae0_dec_fc0_bias_optimizer_adam_moment2_64x1.bin", 272));
  flist.push_back (std::make_pair(base + "train_macc_wae0_dec_fc0_matrix_optimizer_adam_moment1_64x20.bin", 5136));
  flist.push_back (std::make_pair(base + "train_macc_wae0_dec_fc0_matrix_optimizer_adam_moment2_64x20.bin", 5136));
  flist.push_back (std::make_pair(base + "train_macc_wae0_dec_fc1_bias_optimizer_adam_moment1_128x1.bin", 528));
  flist.push_back (std::make_pair(base + "train_macc_wae0_dec_fc1_bias_optimizer_adam_moment2_128x1.bin", 528));
  flist.push_back (std::make_pair(base + "train_macc_wae0_dec_fc1_matrix_optimizer_adam_moment1_128x64.bin", 32784));
  flist.push_back (std::make_pair(base + "train_macc_wae0_dec_fc1_matrix_optimizer_adam_moment2_128x64.bin", 32784));
  flist.push_back (std::make_pair(base + "train_macc_wae0_dec_fc2_bias_optimizer_adam_moment1_256x1.bin", 1040));
  flist.push_back (std::make_pair(base + "train_macc_wae0_dec_fc2_bias_optimizer_adam_moment2_256x1.bin", 1040));
  flist.push_back (std::make_pair(base + "train_macc_wae0_dec_fc2_matrix_optimizer_adam_moment1_256x128.bin", 131088));
  flist.push_back (std::make_pair(base + "train_macc_wae0_dec_fc2_matrix_optimizer_adam_moment2_256x128.bin", 131088));
  flist.push_back (std::make_pair(base + "train_macc_wae0_disc0_fc0_bias_optimizer_adam_moment1_128x1.bin", 528));
  flist.push_back (std::make_pair(base + "train_macc_wae0_disc0_fc0_bias_optimizer_adam_moment2_128x1.bin", 528));
  flist.push_back (std::make_pair(base + "train_macc_wae0_disc0_fc0_matrix_optimizer_adam_moment1_128x16419.bin", 8406544));
  flist.push_back (std::make_pair(base + "train_macc_wae0_disc0_fc0_matrix_optimizer_adam_moment2_128x16419.bin", 8406544));
  flist.push_back (std::make_pair(base + "train_macc_wae0_disc0_fc1_bias_optimizer_adam_moment1_64x1.bin", 272));
  flist.push_back (std::make_pair(base + "train_macc_wae0_disc0_fc1_bias_optimizer_adam_moment2_64x1.bin", 272));
  flist.push_back (std::make_pair(base + "train_macc_wae0_disc0_fc1_matrix_optimizer_adam_moment1_64x128.bin", 32784));
  flist.push_back (std::make_pair(base + "train_macc_wae0_disc0_fc1_matrix_optimizer_adam_moment2_64x128.bin", 32784));
  flist.push_back (std::make_pair(base + "train_macc_wae0_disc0_fc2_bias_optimizer_adam_moment1_1x1.bin", 20));
  flist.push_back (std::make_pair(base + "train_macc_wae0_disc0_fc2_bias_optimizer_adam_moment2_1x1.bin", 20));
  flist.push_back (std::make_pair(base + "train_macc_wae0_disc0_fc2_matrix_optimizer_adam_moment1_1x64.bin", 272));
  flist.push_back (std::make_pair(base + "train_macc_wae0_disc0_fc2_matrix_optimizer_adam_moment2_1x64.bin", 272));
  flist.push_back (std::make_pair(base + "train_macc_wae0_enc_fc0_bias_optimizer_adam_moment1_32x1.bin", 144));
  flist.push_back (std::make_pair(base + "train_macc_wae0_enc_fc0_bias_optimizer_adam_moment2_32x1.bin", 144));
  flist.push_back (std::make_pair(base + "train_macc_wae0_enc_fc0_matrix_optimizer_adam_moment1_32x16399.bin", 2099088));
  flist.push_back (std::make_pair(base + "train_macc_wae0_enc_fc0_matrix_optimizer_adam_moment2_32x16399.bin", 2099088));
  flist.push_back (std::make_pair(base + "train_macc_wae0_enc_fc1_bias_optimizer_adam_moment1_256x1.bin", 1040));
  flist.push_back (std::make_pair(base + "train_macc_wae0_enc_fc1_bias_optimizer_adam_moment2_256x1.bin", 1040));
  flist.push_back (std::make_pair(base + "train_macc_wae0_enc_fc1_matrix_optimizer_adam_moment1_256x32.bin", 32784));
  flist.push_back (std::make_pair(base + "train_macc_wae0_enc_fc1_matrix_optimizer_adam_moment2_256x32.bin", 32784));
  flist.push_back (std::make_pair(base + "train_macc_wae0_enc_fc2_bias_optimizer_adam_moment1_128x1.bin", 528));
  flist.push_back (std::make_pair(base + "train_macc_wae0_enc_fc2_bias_optimizer_adam_moment2_128x1.bin", 528));
  flist.push_back (std::make_pair(base + "train_macc_wae0_enc_fc2_matrix_optimizer_adam_moment1_128x256.bin", 131088));
  flist.push_back (std::make_pair(base + "train_macc_wae0_enc_fc2_matrix_optimizer_adam_moment2_128x256.bin", 131088));
  flist.push_back (std::make_pair(base + "train_macc_wae0enc_out_bias_optimizer_adam_moment1_20x1.bin", 96));
  flist.push_back (std::make_pair(base + "train_macc_wae0enc_out_bias_optimizer_adam_moment2_20x1.bin", 96));
  flist.push_back (std::make_pair(base + "train_macc_wae0enc_out_matrix_optimizer_adam_moment1_20x128.bin", 10256));
  flist.push_back (std::make_pair(base + "train_macc_wae0enc_out_matrix_optimizer_adam_moment2_20x128.bin", 10256));
  flist.push_back (std::make_pair(base + "train_macc_wae0pred_y_bias_optimizer_adam_moment1_16399x1.bin", 65612));
  flist.push_back (std::make_pair(base + "train_macc_wae0pred_y_bias_optimizer_adam_moment2_16399x1.bin", 65612));
  flist.push_back (std::make_pair(base + "train_macc_wae0pred_y_matrix_optimizer_adam_moment1_16399x256.bin", 16792592));
  flist.push_back (std::make_pair(base + "train_macc_wae0pred_y_matrix_optimizer_adam_moment2_16399x256.bin", 16792592));
  flist.push_back (std::make_pair(base + "rng_state/EL_generator", 6704));
  flist.push_back (std::make_pair(base + "rng_state/rng_fast_generator_0_0", 10));
  flist.push_back (std::make_pair(base + "rng_state/rng_fast_generator_0_1", 9));
  flist.push_back (std::make_pair(base + "rng_state/rng_fast_generator_0_2", 9));
  flist.push_back (std::make_pair(base + "rng_state/rng_fast_generator_0_3", 9));
  flist.push_back (std::make_pair(base + "rng_state/rng_fast_generator_0_4", 9));
  flist.push_back (std::make_pair(base + "rng_state/rng_fast_generator_0_5", 9));
  flist.push_back (std::make_pair(base + "rng_state/rng_fast_generator_0_6", 9));
  flist.push_back (std::make_pair(base + "rng_state/rng_fast_generator_0_7", 9));
  flist.push_back (std::make_pair(base + "rng_state/rng_fast_generator_1_0", 9));
  flist.push_back (std::make_pair(base + "rng_state/rng_fast_generator_1_1", 9));
  flist.push_back (std::make_pair(base + "rng_state/rng_fast_generator_1_2", 9));
  flist.push_back (std::make_pair(base + "rng_state/rng_fast_generator_1_3", 9));
  flist.push_back (std::make_pair(base + "rng_state/rng_fast_generator_1_4", 9));
  flist.push_back (std::make_pair(base + "rng_state/rng_fast_generator_1_5", 9));
  flist.push_back (std::make_pair(base + "rng_state/rng_fast_generator_1_6", 9));
  flist.push_back (std::make_pair(base + "rng_state/rng_fast_generator_1_7", 9));
  flist.push_back (std::make_pair(base + "rng_state/rng_fast_generator_2_0", 9));
  flist.push_back (std::make_pair(base + "rng_state/rng_fast_generator_2_1", 9));
  flist.push_back (std::make_pair(base + "rng_state/rng_fast_generator_2_2", 9));
  flist.push_back (std::make_pair(base + "rng_state/rng_fast_generator_2_3", 9));
  flist.push_back (std::make_pair(base + "rng_state/rng_fast_generator_2_4", 9));
  flist.push_back (std::make_pair(base + "rng_state/rng_fast_generator_2_5", 9));
  flist.push_back (std::make_pair(base + "rng_state/rng_fast_generator_2_6", 9));
  flist.push_back (std::make_pair(base + "rng_state/rng_fast_generator_2_7", 9));
  flist.push_back (std::make_pair(base + "rng_state/rng_fast_generator_3_0", 9));
  flist.push_back (std::make_pair(base + "rng_state/rng_fast_generator_3_1", 9));
  flist.push_back (std::make_pair(base + "rng_state/rng_fast_generator_3_2", 9));
  flist.push_back (std::make_pair(base + "rng_state/rng_fast_generator_3_3", 9));
  flist.push_back (std::make_pair(base + "rng_state/rng_fast_generator_3_4", 9));
  flist.push_back (std::make_pair(base + "rng_state/rng_fast_generator_3_5", 9));
  flist.push_back (std::make_pair(base + "rng_state/rng_fast_generator_3_6", 9));
  flist.push_back (std::make_pair(base + "rng_state/rng_fast_generator_3_7", 9));
  flist.push_back (std::make_pair(base + "rng_state/rng_fast_generator_4_0", 9));
  flist.push_back (std::make_pair(base + "rng_state/rng_fast_generator_4_1", 9));
  flist.push_back (std::make_pair(base + "rng_state/rng_fast_generator_4_2", 9));
  flist.push_back (std::make_pair(base + "rng_state/rng_fast_generator_4_3", 9));
  flist.push_back (std::make_pair(base + "rng_state/rng_fast_generator_4_4", 9));
  flist.push_back (std::make_pair(base + "rng_state/rng_fast_generator_4_5", 9));
  flist.push_back (std::make_pair(base + "rng_state/rng_fast_generator_4_6", 9));
  flist.push_back (std::make_pair(base + "rng_state/rng_fast_generator_4_7", 9));
  flist.push_back (std::make_pair(base + "rng_state/rng_fast_generator_5_0", 9));
  flist.push_back (std::make_pair(base + "rng_state/rng_fast_generator_5_1", 9));
  flist.push_back (std::make_pair(base + "rng_state/rng_fast_generator_5_2", 9));
  flist.push_back (std::make_pair(base + "rng_state/rng_fast_generator_5_3", 9));
  flist.push_back (std::make_pair(base + "rng_state/rng_fast_generator_5_4", 9));
  flist.push_back (std::make_pair(base + "rng_state/rng_fast_generator_5_5", 9));
  flist.push_back (std::make_pair(base + "rng_state/rng_fast_generator_5_6", 9));
  flist.push_back (std::make_pair(base + "rng_state/rng_fast_generator_5_7", 9));
  flist.push_back (std::make_pair(base + "rng_state/rng_fast_generator_6_0", 9));
  flist.push_back (std::make_pair(base + "rng_state/rng_fast_generator_6_1", 9));
  flist.push_back (std::make_pair(base + "rng_state/rng_fast_generator_6_2", 9));
  flist.push_back (std::make_pair(base + "rng_state/rng_fast_generator_6_3", 9));
  flist.push_back (std::make_pair(base + "rng_state/rng_fast_generator_6_4", 9));
  flist.push_back (std::make_pair(base + "rng_state/rng_fast_generator_6_5", 9));
  flist.push_back (std::make_pair(base + "rng_state/rng_fast_generator_6_6", 9));
  flist.push_back (std::make_pair(base + "rng_state/rng_fast_generator_6_7", 9));
  flist.push_back (std::make_pair(base + "rng_state/rng_fast_generator_7_0", 9));
  flist.push_back (std::make_pair(base + "rng_state/rng_fast_generator_7_1", 9));
  flist.push_back (std::make_pair(base + "rng_state/rng_fast_generator_7_2", 9));
  flist.push_back (std::make_pair(base + "rng_state/rng_fast_generator_7_3", 9));
  flist.push_back (std::make_pair(base + "rng_state/rng_fast_generator_7_4", 9));
  flist.push_back (std::make_pair(base + "rng_state/rng_fast_generator_7_5", 9));
  flist.push_back (std::make_pair(base + "rng_state/rng_fast_generator_7_6", 9));
  flist.push_back (std::make_pair(base + "rng_state/rng_fast_generator_7_7", 9));
  flist.push_back (std::make_pair(base + "rng_state/rng_fast_io_generator_0", 1));
  flist.push_back (std::make_pair(base + "rng_state/rng_fast_io_generator_1", 1));
  flist.push_back (std::make_pair(base + "rng_state/rng_fast_io_generator_2", 1));
  flist.push_back (std::make_pair(base + "rng_state/rng_fast_io_generator_3", 1));
  flist.push_back (std::make_pair(base + "rng_state/rng_fast_io_generator_4", 1));
  flist.push_back (std::make_pair(base + "rng_state/rng_fast_io_generator_5", 1));
  flist.push_back (std::make_pair(base + "rng_state/rng_fast_io_generator_6", 1));
  flist.push_back (std::make_pair(base + "rng_state/rng_fast_io_generator_7", 1));
  flist.push_back (std::make_pair(base + "rng_state/rng_generator_0_0", 6708));
  flist.push_back (std::make_pair(base + "rng_state/rng_generator_0_1", 6724));
  flist.push_back (std::make_pair(base + "rng_state/rng_generator_0_2", 6737));
  flist.push_back (std::make_pair(base + "rng_state/rng_generator_0_3", 6727));
  flist.push_back (std::make_pair(base + "rng_state/rng_generator_0_4", 6709));
  flist.push_back (std::make_pair(base + "rng_state/rng_generator_0_5", 6705));
  flist.push_back (std::make_pair(base + "rng_state/rng_generator_0_6", 6701));
  flist.push_back (std::make_pair(base + "rng_state/rng_generator_0_7", 6706));
  flist.push_back (std::make_pair(base + "rng_state/rng_generator_1_0", 6725));
  flist.push_back (std::make_pair(base + "rng_state/rng_generator_1_1", 6705));
  flist.push_back (std::make_pair(base + "rng_state/rng_generator_1_2", 6705));
  flist.push_back (std::make_pair(base + "rng_state/rng_generator_1_3", 6694));
  flist.push_back (std::make_pair(base + "rng_state/rng_generator_1_4", 6725));
  flist.push_back (std::make_pair(base + "rng_state/rng_generator_1_5", 6686));
  flist.push_back (std::make_pair(base + "rng_state/rng_generator_1_6", 6703));
  flist.push_back (std::make_pair(base + "rng_state/rng_generator_1_7", 6684));
  flist.push_back (std::make_pair(base + "rng_state/rng_generator_2_0", 6716));
  flist.push_back (std::make_pair(base + "rng_state/rng_generator_2_1", 6717));
  flist.push_back (std::make_pair(base + "rng_state/rng_generator_2_2", 6738));
  flist.push_back (std::make_pair(base + "rng_state/rng_generator_2_3", 6707));
  flist.push_back (std::make_pair(base + "rng_state/rng_generator_2_4", 6705));
  flist.push_back (std::make_pair(base + "rng_state/rng_generator_2_5", 6703));
  flist.push_back (std::make_pair(base + "rng_state/rng_generator_2_6", 6696));
  flist.push_back (std::make_pair(base + "rng_state/rng_generator_2_7", 6702));
  flist.push_back (std::make_pair(base + "rng_state/rng_generator_3_0", 6707));
  flist.push_back (std::make_pair(base + "rng_state/rng_generator_3_1", 6701));
  flist.push_back (std::make_pair(base + "rng_state/rng_generator_3_2", 6706));
  flist.push_back (std::make_pair(base + "rng_state/rng_generator_3_3", 6706));
  flist.push_back (std::make_pair(base + "rng_state/rng_generator_3_4", 6717));
  flist.push_back (std::make_pair(base + "rng_state/rng_generator_3_5", 6693));
  flist.push_back (std::make_pair(base + "rng_state/rng_generator_3_6", 6678));
  flist.push_back (std::make_pair(base + "rng_state/rng_generator_3_7", 6714));
  flist.push_back (std::make_pair(base + "rng_state/rng_generator_4_0", 6694));
  flist.push_back (std::make_pair(base + "rng_state/rng_generator_4_1", 6716));
  flist.push_back (std::make_pair(base + "rng_state/rng_generator_4_2", 6695));
  flist.push_back (std::make_pair(base + "rng_state/rng_generator_4_3", 6700));
  flist.push_back (std::make_pair(base + "rng_state/rng_generator_4_4", 6699));
  flist.push_back (std::make_pair(base + "rng_state/rng_generator_4_5", 6708));
  flist.push_back (std::make_pair(base + "rng_state/rng_generator_4_6", 6706));
  flist.push_back (std::make_pair(base + "rng_state/rng_generator_4_7", 6702));
  flist.push_back (std::make_pair(base + "rng_state/rng_generator_5_0", 6708));
  flist.push_back (std::make_pair(base + "rng_state/rng_generator_5_1", 6706));
  flist.push_back (std::make_pair(base + "rng_state/rng_generator_5_2", 6699));
  flist.push_back (std::make_pair(base + "rng_state/rng_generator_5_3", 6702));
  flist.push_back (std::make_pair(base + "rng_state/rng_generator_5_4", 6717));
  flist.push_back (std::make_pair(base + "rng_state/rng_generator_5_5", 6674));
  flist.push_back (std::make_pair(base + "rng_state/rng_generator_5_6", 6704));
  flist.push_back (std::make_pair(base + "rng_state/rng_generator_5_7", 6704));
  flist.push_back (std::make_pair(base + "rng_state/rng_generator_6_0", 6715));
  flist.push_back (std::make_pair(base + "rng_state/rng_generator_6_1", 6689));
  flist.push_back (std::make_pair(base + "rng_state/rng_generator_6_2", 6712));
  flist.push_back (std::make_pair(base + "rng_state/rng_generator_6_3", 6721));
  flist.push_back (std::make_pair(base + "rng_state/rng_generator_6_4", 6708));
  flist.push_back (std::make_pair(base + "rng_state/rng_generator_6_5", 6712));
  flist.push_back (std::make_pair(base + "rng_state/rng_generator_6_6", 6681));
  flist.push_back (std::make_pair(base + "rng_state/rng_generator_6_7", 6701));
  flist.push_back (std::make_pair(base + "rng_state/rng_generator_7_0", 6687));
  flist.push_back (std::make_pair(base + "rng_state/rng_generator_7_1", 6702));
  flist.push_back (std::make_pair(base + "rng_state/rng_generator_7_2", 6721));
  flist.push_back (std::make_pair(base + "rng_state/rng_generator_7_3", 6708));
  flist.push_back (std::make_pair(base + "rng_state/rng_generator_7_4", 6715));
  flist.push_back (std::make_pair(base + "rng_state/rng_generator_7_5", 6699));
  flist.push_back (std::make_pair(base + "rng_state/rng_generator_7_6", 6713));
  flist.push_back (std::make_pair(base + "rng_state/rng_generator_7_7", 6706));
  flist.push_back (std::make_pair(base + "rng_state/rng_io_generator_0", 6694));
  flist.push_back (std::make_pair(base + "rng_state/rng_io_generator_1", 6694));
  flist.push_back (std::make_pair(base + "rng_state/rng_io_generator_2", 6694));
  flist.push_back (std::make_pair(base + "rng_state/rng_io_generator_3", 6694));
  flist.push_back (std::make_pair(base + "rng_state/rng_io_generator_4", 6694));
  flist.push_back (std::make_pair(base + "rng_state/rng_io_generator_5", 6694));
  flist.push_back (std::make_pair(base + "rng_state/rng_io_generator_6", 6694));
  flist.push_back (std::make_pair(base + "rng_state/rng_io_generator_7", 6694));
  flist.push_back (std::make_pair(base + "rng_state/rng_seq_generator", 6712));
}

void set_flist (std::string base, flist_t& flist,
                const size_t num_files, const size_t fixed_size)
{
  flist.clear ();
  if (!base.empty() && (base.back () != '/')) {
      base += '/';
  }
  const std::string size_str = '.' + std::to_string (fixed_size);
  for (size_t i = 0ul; i < num_files ; ++i) {
      flist.push_back (std::make_pair (base + "data_file_" + std::to_string (i)
                                            + size_str + ".bin", fixed_size));
  }
}
