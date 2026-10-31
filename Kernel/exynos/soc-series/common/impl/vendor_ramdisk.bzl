# Copyright (C) 2022 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#       http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

load("//build/kernel/kleaf:hermetic_tools.bzl", "hermetic_toolchain")

def _create_vendor_ramdisk_impl(ctx):
    vendor_ramdisk_archive = ctx.actions.declare_file("{}/vendor_ramdisk.cpio".format(ctx.label.name))
    modules_staging_dir = vendor_ramdisk_archive.dirname + "/staging"
    vendor_ramdisk_staging_dir = modules_staging_dir + "/vendor_ramdisk_staging"

    inputs = []
    transitive_inputs = []
    hermetic_tools = hermetic_toolchain.get(ctx)
    inputs += ctx.files.vendor_files
    inputs += ctx.files.vendor_ramdisk_list
    inputs += ctx.files.system_prebuilt_files

    command = hermetic_tools.setup + """
        mkdir -p {vendor_ramdisk_staging_dir}
        mkdir -p {vendor_ramdisk_staging_dir}/first_stage_ramdisk
        mkdir -p {vendor_ramdisk_staging_dir}/etc/init
        mkdir -p {vendor_ramdisk_staging_dir}/system
    """.format(
        vendor_ramdisk_staging_dir = vendor_ramdisk_staging_dir,
    )

    for vendor_file in ctx.files.vendor_files:
        command += """
            for m in $(egrep -e ^"{src_file}: " {vendor_ramdisk_list_file}); do
              if [[ "$m" != *":"* ]]; then
                # ignore source file name which includes ":"
                echo $m
                mkdir -p {vendor_ramdisk_staging_dir}/$(dirname $m)
                cp -f {vendor_file} {vendor_ramdisk_staging_dir}/$(dirname $m)/$(basename $m)
              fi
            done
        """.format(
            src_file = vendor_file.basename,
            vendor_ramdisk_list_file = " ".join([f.path for f in ctx.files.vendor_ramdisk_list]),
            vendor_file = vendor_file.path,
            vendor_ramdisk_staging_dir = vendor_ramdisk_staging_dir,
        )

    for prebuilt_file in ctx.files.system_prebuilt_files:
        command += """
            tar -xvf {prebuilt_file} -C {vendor_ramdisk_staging_dir}/system/
        """.format(
            prebuilt_file = prebuilt_file.path,
            vendor_ramdisk_staging_dir = vendor_ramdisk_staging_dir,
        )

    command += """
        mkbootfs "{vendor_ramdisk_staging_dir}" >{output}
    """.format(
        vendor_ramdisk_staging_dir = vendor_ramdisk_staging_dir,
        output = vendor_ramdisk_archive.path,
    )

    ctx.actions.run_shell(
        mnemonic = "VendorRamdisk",
        progress_message = "Create vendor ramdisk.cpio",
        inputs = depset(inputs, transitive = transitive_inputs),
        tools = hermetic_tools.deps,
        outputs = [vendor_ramdisk_archive],
        command = command,
    )
    return DefaultInfo(files = depset([vendor_ramdisk_archive]))

create_vendor_ramdisk = rule(
    implementation = _create_vendor_ramdisk_impl,
    doc = """Create Vendor ramdisk binary.
    """,
    attrs = {
        "vendor_files": attr.label_list(
            allow_files = True,
        ),
        "vendor_ramdisk_list": attr.label_list(
            doc = "A list of target location in ramdisk for source files",
            allow_files = True,
            mandatory = True,
        ),
        "system_prebuilt_files": attr.label_list(
            doc = "A list fo prebuilt files under /system. Please use tar.gz",
            allow_files = True,
        ),
    },
    toolchains = [hermetic_toolchain.type],
)
