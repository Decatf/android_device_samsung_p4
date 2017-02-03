# Copyright (C) 2011 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

$(call inherit-product, $(SRC_TARGET_DIR)/product/aosp_base_telephony.mk)

# The gps config appropriate for this device
$(call inherit-product, device/common/gps/gps_us_supl.mk)

$(call inherit-product, device/samsung/p4-common/p4-common.mk)
$(call inherit-product, device/samsung/p4/device.mk)
$(call inherit-product-if-exists, vendor/samsung/p4/p4-vendor.mk)

PRODUCT_NAME := p4
PRODUCT_BRAND := Samsung
PRODUCT_DEVICE := p4
PRODUCT_MODEL := GT-P7500
PRODUCT_MANUFACTURER := Samsung
PRODUCT_BUILD_PROP_OVERRIDES += \
	PRODUCT_NAME=GT-P7500 \
	BUILD_FINGERPRINT="samsung/GT-P7500/GT-P7500:4.0.4/IMM76D/XXLQ8:user/release-keys" \
	PRIVATE_BUILD_DESC="GT-P7500-user 4.0.4 IMM76D XXLQ8 release-keys"
