/**
 * Copyright 2020-2021 Huawei Technologies Co., Ltd
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "minddata/dataset/include/vision.h"

#include "minddata/dataset/include/transforms.h"
#include "minddata/dataset/kernels/ir/vision/vision_ir.h"

#ifndef ENABLE_ANDROID
#include "minddata/dataset/kernels/image/image_utils.h"
#endif
#include "minddata/dataset/kernels/ir/validators.h"

// Kernel image headers (in alphabetical order)
#ifndef ENABLE_ANDROID
#include "minddata/dataset/kernels/image/auto_contrast_op.h"
#include "minddata/dataset/kernels/image/bounding_box_augment_op.h"
#endif
#include "minddata/dataset/kernels/image/center_crop_op.h"
#include "minddata/dataset/kernels/image/crop_op.h"
#ifndef ENABLE_ANDROID
#include "minddata/dataset/kernels/image/cutmix_batch_op.h"
#include "minddata/dataset/kernels/image/cut_out_op.h"
#endif
#include "minddata/dataset/kernels/image/decode_op.h"
#ifdef ENABLE_ACL
#include "minddata/dataset/kernels/image/dvpp/dvpp_decode_resize_crop_jpeg_op.h"
#endif
#ifndef ENABLE_ANDROID
#include "minddata/dataset/kernels/image/equalize_op.h"
#include "minddata/dataset/kernels/image/hwc_to_chw_op.h"
#include "minddata/dataset/kernels/image/invert_op.h"
#include "minddata/dataset/kernels/image/mixup_batch_op.h"
#endif
#include "minddata/dataset/kernels/image/normalize_op.h"
#include "minddata/dataset/kernels/image/normalize_pad_op.h"
#ifndef ENABLE_ANDROID
#include "minddata/dataset/kernels/image/pad_op.h"
#include "minddata/dataset/kernels/image/random_affine_op.h"
#include "minddata/dataset/kernels/image/random_color_op.h"
#include "minddata/dataset/kernels/image/random_color_adjust_op.h"
#include "minddata/dataset/kernels/image/random_crop_and_resize_op.h"
#include "minddata/dataset/kernels/image/random_crop_op.h"
#include "minddata/dataset/kernels/image/random_crop_decode_resize_op.h"
#include "minddata/dataset/kernels/image/random_crop_with_bbox_op.h"
#include "minddata/dataset/kernels/image/random_crop_and_resize_with_bbox_op.h"
#include "minddata/dataset/kernels/image/random_horizontal_flip_op.h"
#include "minddata/dataset/kernels/image/random_horizontal_flip_with_bbox_op.h"
#include "minddata/dataset/kernels/image/random_posterize_op.h"
#include "minddata/dataset/kernels/image/random_resize_op.h"
#include "minddata/dataset/kernels/image/random_resize_with_bbox_op.h"
#include "minddata/dataset/kernels/image/random_rotation_op.h"
#include "minddata/dataset/kernels/image/random_select_subpolicy_op.h"
#include "minddata/dataset/kernels/image/random_sharpness_op.h"
#include "minddata/dataset/kernels/image/random_solarize_op.h"
#include "minddata/dataset/kernels/image/random_vertical_flip_op.h"
#include "minddata/dataset/kernels/image/random_vertical_flip_with_bbox_op.h"
#include "minddata/dataset/kernels/image/rescale_op.h"
#endif
#include "minddata/dataset/kernels/image/resize_op.h"
#ifndef ENABLE_ANDROID
#include "minddata/dataset/kernels/image/resize_with_bbox_op.h"
#include "minddata/dataset/kernels/image/rgba_to_bgr_op.h"
#include "minddata/dataset/kernels/image/rgba_to_rgb_op.h"
#include "minddata/dataset/kernels/image/soft_dvpp/soft_dvpp_decode_random_crop_resize_jpeg_op.h"
#include "minddata/dataset/kernels/image/soft_dvpp/soft_dvpp_decode_resize_jpeg_op.h"
#include "minddata/dataset/kernels/image/swap_red_blue_op.h"
#include "minddata/dataset/kernels/image/uniform_aug_op.h"
#endif
#include "minddata/dataset/kernels/image/rotate_op.h"

namespace mindspore {
namespace dataset {

// Transform operations for computer vision.
namespace vision {
#ifndef ENABLE_ANDROID
// FUNCTIONS TO CREATE VISION TRANSFORM OPERATIONS
// (In alphabetical order)

// Function to create AutoContrastOperation.
std::shared_ptr<AutoContrastOperation> AutoContrast(float cutoff, std::vector<uint32_t> ignore) {
  auto op = std::make_shared<AutoContrastOperation>(cutoff, ignore);
  // Input validation
  return op->ValidateParams() ? op : nullptr;
}

// Function to create BoundingBoxAugmentOperation.
std::shared_ptr<BoundingBoxAugmentOperation> BoundingBoxAugment(std::shared_ptr<TensorOperation> transform,
                                                                float ratio) {
  auto op = std::make_shared<BoundingBoxAugmentOperation>(transform, ratio);
  // Input validation
  return op->ValidateParams() ? op : nullptr;
}
#endif

// Function to create CenterCropOperation.
std::shared_ptr<CenterCropOperation> CenterCrop(std::vector<int32_t> size) {
  auto op = std::make_shared<CenterCropOperation>(size);
  // Input validation
  return op->ValidateParams() ? op : nullptr;
}

// Function to create CropOperation.
std::shared_ptr<CropOperation> Crop(std::vector<int32_t> coordinates, std::vector<int32_t> size) {
  auto op = std::make_shared<CropOperation>(coordinates, size);
  // Input validation
  return op->ValidateParams() ? op : nullptr;
}
#ifndef ENABLE_ANDROID
// Function to create CutMixBatchOperation.
std::shared_ptr<CutMixBatchOperation> CutMixBatch(ImageBatchFormat image_batch_format, float alpha, float prob) {
  auto op = std::make_shared<CutMixBatchOperation>(image_batch_format, alpha, prob);
  // Input validation
  return op->ValidateParams() ? op : nullptr;
}

// Function to create CutOutOp.
std::shared_ptr<CutOutOperation> CutOut(int32_t length, int32_t num_patches) {
  auto op = std::make_shared<CutOutOperation>(length, num_patches);
  // Input validation
  return op->ValidateParams() ? op : nullptr;
}

// Function to create DecodeOperation.
std::shared_ptr<DecodeOperation> Decode(bool rgb) {
  auto op = std::make_shared<DecodeOperation>(rgb);
  // Input validation
  return op->ValidateParams() ? op : nullptr;
}

#ifdef ENABLE_ACL
// Function to create DvppDecodeResizeCropOperation.
std::shared_ptr<DvppDecodeResizeCropOperation> DvppDecodeResizeCropJpeg(std::vector<uint32_t> crop,
                                                                        std::vector<uint32_t> resize) {
  auto op = std::make_shared<DvppDecodeResizeCropOperation>(crop, resize);
  // Input validation
  return op->ValidateParams() ? op : nullptr;
}
#endif

// Function to create EqualizeOperation.
std::shared_ptr<EqualizeOperation> Equalize() {
  auto op = std::make_shared<EqualizeOperation>();
  // Input validation
  return op->ValidateParams() ? op : nullptr;
}

// Function to create HwcToChwOperation.
std::shared_ptr<HwcToChwOperation> HWC2CHW() {
  auto op = std::make_shared<HwcToChwOperation>();
  // Input validation
  return op->ValidateParams() ? op : nullptr;
}

// Function to create InvertOperation.
std::shared_ptr<InvertOperation> Invert() {
  auto op = std::make_shared<InvertOperation>();
  // Input validation
  return op->ValidateParams() ? op : nullptr;
}

// Function to create MixUpBatchOperation.
std::shared_ptr<MixUpBatchOperation> MixUpBatch(float alpha) {
  auto op = std::make_shared<MixUpBatchOperation>(alpha);
  // Input validation
  return op->ValidateParams() ? op : nullptr;
}
#endif

// Function to create NormalizeOperation.
std::shared_ptr<NormalizeOperation> Normalize(std::vector<float> mean, std::vector<float> std) {
  auto op = std::make_shared<NormalizeOperation>(mean, std);
  // Input validation
  return op->ValidateParams() ? op : nullptr;
}

#ifndef ENABLE_ANDROID
// Function to create NormalizePadOperation.
std::shared_ptr<NormalizePadOperation> NormalizePad(const std::vector<float> &mean, const std::vector<float> &std,
                                                    const std::string &dtype) {
  auto op = std::make_shared<NormalizePadOperation>(mean, std, dtype);
  // Input validation
  return op->ValidateParams() ? op : nullptr;
}

// Function to create PadOperation.
std::shared_ptr<PadOperation> Pad(std::vector<int32_t> padding, std::vector<uint8_t> fill_value,
                                  BorderType padding_mode) {
  auto op = std::make_shared<PadOperation>(padding, fill_value, padding_mode);
  // Input validation
  return op->ValidateParams() ? op : nullptr;
}

// Function to create RandomAffineOperation.
std::shared_ptr<RandomAffineOperation> RandomAffine(const std::vector<float_t> &degrees,
                                                    const std::vector<float_t> &translate_range,
                                                    const std::vector<float_t> &scale_range,
                                                    const std::vector<float_t> &shear_ranges,
                                                    InterpolationMode interpolation,
                                                    const std::vector<uint8_t> &fill_value) {
  auto op = std::make_shared<RandomAffineOperation>(degrees, translate_range, scale_range, shear_ranges, interpolation,
                                                    fill_value);
  // Input validation
  return op->ValidateParams() ? op : nullptr;
}

// Function to create RandomColorOperation.
std::shared_ptr<RandomColorOperation> RandomColor(float t_lb, float t_ub) {
  auto op = std::make_shared<RandomColorOperation>(t_lb, t_ub);
  // Input validation
  return op->ValidateParams() ? op : nullptr;
}

std::shared_ptr<TensorOp> RandomColorOperation::Build() {
  std::shared_ptr<RandomColorOp> tensor_op = std::make_shared<RandomColorOp>(t_lb_, t_ub_);
  return tensor_op;
}

// Function to create RandomColorAdjustOperation.
std::shared_ptr<RandomColorAdjustOperation> RandomColorAdjust(std::vector<float> brightness,
                                                              std::vector<float> contrast,
                                                              std::vector<float> saturation, std::vector<float> hue) {
  auto op = std::make_shared<RandomColorAdjustOperation>(brightness, contrast, saturation, hue);
  // Input validation
  return op->ValidateParams() ? op : nullptr;
}

// Function to create RandomCropOperation.
std::shared_ptr<RandomCropOperation> RandomCrop(std::vector<int32_t> size, std::vector<int32_t> padding,
                                                bool pad_if_needed, std::vector<uint8_t> fill_value,
                                                BorderType padding_mode) {
  auto op = std::make_shared<RandomCropOperation>(size, padding, pad_if_needed, fill_value, padding_mode);
  // Input validation
  return op->ValidateParams() ? op : nullptr;
}

// Function to create RandomCropDecodeResizeOperation.
std::shared_ptr<RandomCropDecodeResizeOperation> RandomCropDecodeResize(std::vector<int32_t> size,
                                                                        std::vector<float> scale,
                                                                        std::vector<float> ratio,
                                                                        InterpolationMode interpolation,
                                                                        int32_t max_attempts) {
  auto op = std::make_shared<RandomCropDecodeResizeOperation>(size, scale, ratio, interpolation, max_attempts);
  // Input validation
  return op->ValidateParams() ? op : nullptr;
}

// Function to create RandomCropWithBBoxOperation.
std::shared_ptr<RandomCropWithBBoxOperation> RandomCropWithBBox(std::vector<int32_t> size, std::vector<int32_t> padding,
                                                                bool pad_if_needed, std::vector<uint8_t> fill_value,
                                                                BorderType padding_mode) {
  auto op = std::make_shared<RandomCropWithBBoxOperation>(size, padding, pad_if_needed, fill_value, padding_mode);
  // Input validation
  return op->ValidateParams() ? op : nullptr;
}

// Function to create RandomHorizontalFlipOperation.
std::shared_ptr<RandomHorizontalFlipOperation> RandomHorizontalFlip(float prob) {
  auto op = std::make_shared<RandomHorizontalFlipOperation>(prob);
  // Input validation
  return op->ValidateParams() ? op : nullptr;
}

// Function to create RandomHorizontalFlipOperation.
std::shared_ptr<RandomHorizontalFlipWithBBoxOperation> RandomHorizontalFlipWithBBox(float prob) {
  auto op = std::make_shared<RandomHorizontalFlipWithBBoxOperation>(prob);
  // Input validation
  return op->ValidateParams() ? op : nullptr;
}

// Function to create RandomPosterizeOperation.
std::shared_ptr<RandomPosterizeOperation> RandomPosterize(const std::vector<uint8_t> &bit_range) {
  auto op = std::make_shared<RandomPosterizeOperation>(bit_range);
  // Input validation
  return op->ValidateParams() ? op : nullptr;
}

// Function to create RandomResizeOperation.
std::shared_ptr<RandomResizeOperation> RandomResize(std::vector<int32_t> size) {
  auto op = std::make_shared<RandomResizeOperation>(size);
  // Input validation
  return op->ValidateParams() ? op : nullptr;
}

// Function to create RandomResizeWithBBoxOperation.
std::shared_ptr<RandomResizeWithBBoxOperation> RandomResizeWithBBox(std::vector<int32_t> size) {
  auto op = std::make_shared<RandomResizeWithBBoxOperation>(size);
  // Input validation
  return op->ValidateParams() ? op : nullptr;
}

// Function to create RandomResizedCropOperation.
std::shared_ptr<RandomResizedCropOperation> RandomResizedCrop(std::vector<int32_t> size, std::vector<float> scale,
                                                              std::vector<float> ratio, InterpolationMode interpolation,
                                                              int32_t max_attempts) {
  auto op = std::make_shared<RandomResizedCropOperation>(size, scale, ratio, interpolation, max_attempts);
  // Input validation
  return op->ValidateParams() ? op : nullptr;
}

// Function to create RandomResizedCropOperation.
std::shared_ptr<RandomResizedCropWithBBoxOperation> RandomResizedCropWithBBox(std::vector<int32_t> size,
                                                                              std::vector<float> scale,
                                                                              std::vector<float> ratio,
                                                                              InterpolationMode interpolation,
                                                                              int32_t max_attempts) {
  auto op = std::make_shared<RandomResizedCropWithBBoxOperation>(size, scale, ratio, interpolation, max_attempts);
  // Input validation
  return op->ValidateParams() ? op : nullptr;
}

// Function to create RandomRotationOperation.
std::shared_ptr<RandomRotationOperation> RandomRotation(std::vector<float> degrees, InterpolationMode resample,
                                                        bool expand, std::vector<float> center,
                                                        std::vector<uint8_t> fill_value) {
  auto op = std::make_shared<RandomRotationOperation>(degrees, resample, expand, center, fill_value);
  // Input validation
  return op->ValidateParams() ? op : nullptr;
}

// Function to create RandomSharpnessOperation.
std::shared_ptr<RandomSharpnessOperation> RandomSharpness(std::vector<float> degrees) {
  auto op = std::make_shared<RandomSharpnessOperation>(degrees);
  // Input validation
  return op->ValidateParams() ? op : nullptr;
}

// Function to create RandomSolarizeOperation.
std::shared_ptr<RandomSolarizeOperation> RandomSolarize(std::vector<uint8_t> threshold) {
  auto op = std::make_shared<RandomSolarizeOperation>(threshold);
  // Input validation
  return op->ValidateParams() ? op : nullptr;
}

// Function to create RandomSelectSubpolicyOperation.
std::shared_ptr<RandomSelectSubpolicyOperation> RandomSelectSubpolicy(
  std::vector<std::vector<std::pair<std::shared_ptr<TensorOperation>, double>>> policy) {
  auto op = std::make_shared<RandomSelectSubpolicyOperation>(policy);
  // Input validation
  return op->ValidateParams() ? op : nullptr;
}

// Function to create RandomVerticalFlipOperation.
std::shared_ptr<RandomVerticalFlipOperation> RandomVerticalFlip(float prob) {
  auto op = std::make_shared<RandomVerticalFlipOperation>(prob);
  // Input validation
  return op->ValidateParams() ? op : nullptr;
}

// Function to create RandomVerticalFlipWithBBoxOperation.
std::shared_ptr<RandomVerticalFlipWithBBoxOperation> RandomVerticalFlipWithBBox(float prob) {
  auto op = std::make_shared<RandomVerticalFlipWithBBoxOperation>(prob);
  // Input validation
  return op->ValidateParams() ? op : nullptr;
}

// Function to create RescaleOperation.
std::shared_ptr<RescaleOperation> Rescale(float rescale, float shift) {
  auto op = std::make_shared<RescaleOperation>(rescale, shift);
  // Input validation
  return op->ValidateParams() ? op : nullptr;
}

#endif
// Function to create ResizeOperation.
std::shared_ptr<ResizeOperation> Resize(std::vector<int32_t> size, InterpolationMode interpolation) {
  auto op = std::make_shared<ResizeOperation>(size, interpolation);
  // Input validation
  return op->ValidateParams() ? op : nullptr;
}

#ifdef ENABLE_ANDROID
// Function to create RotateOperation.
std::shared_ptr<RotateOperation> Rotate() {
  auto op = std::make_shared<RotateOperation>();
  // Input validation
  return op->ValidateParams() ? op : nullptr;
}
#endif

#ifndef ENABLE_ANDROID
// Function to create ResizeWithBBoxOperation.
std::shared_ptr<ResizeWithBBoxOperation> ResizeWithBBox(std::vector<int32_t> size, InterpolationMode interpolation) {
  auto op = std::make_shared<ResizeWithBBoxOperation>(size, interpolation);
  // Input validation
  return op->ValidateParams() ? op : nullptr;
}

// Function to create RgbaToBgrOperation.
std::shared_ptr<RgbaToBgrOperation> RGBA2BGR() {
  auto op = std::make_shared<RgbaToBgrOperation>();
  // Input validation
  return op->ValidateParams() ? op : nullptr;
}

// Function to create RgbaToRgbOperation.
std::shared_ptr<RgbaToRgbOperation> RGBA2RGB() {
  auto op = std::make_shared<RgbaToRgbOperation>();
  // Input validation
  return op->ValidateParams() ? op : nullptr;
}

// Function to create SoftDvppDecodeRandomCropResizeJpegOperation.
std::shared_ptr<SoftDvppDecodeRandomCropResizeJpegOperation> SoftDvppDecodeRandomCropResizeJpeg(
  std::vector<int32_t> size, std::vector<float> scale, std::vector<float> ratio, int32_t max_attempts) {
  auto op = std::make_shared<SoftDvppDecodeRandomCropResizeJpegOperation>(size, scale, ratio, max_attempts);
  // Input validation
  return op->ValidateParams() ? op : nullptr;
}

// Function to create SoftDvppDecodeResizeJpegOperation.
std::shared_ptr<SoftDvppDecodeResizeJpegOperation> SoftDvppDecodeResizeJpeg(std::vector<int32_t> size) {
  auto op = std::make_shared<SoftDvppDecodeResizeJpegOperation>(size);
  // Input validation
  return op->ValidateParams() ? op : nullptr;
}

// Function to create SwapRedBlueOperation.
std::shared_ptr<SwapRedBlueOperation> SwapRedBlue() {
  auto op = std::make_shared<SwapRedBlueOperation>();
  // Input validation
  return op->ValidateParams() ? op : nullptr;
}

// Function to create UniformAugOperation.
std::shared_ptr<UniformAugOperation> UniformAugment(std::vector<std::shared_ptr<TensorOperation>> transforms,
                                                    int32_t num_ops) {
  auto op = std::make_shared<UniformAugOperation>(transforms, num_ops);
  // Input validation
  return op->ValidateParams() ? op : nullptr;
}
#endif

}  // namespace vision
}  // namespace dataset
}  // namespace mindspore
