package=blake3
$(package)_version=1.8.1
$(package)_download_path=https://github.com/BLAKE3-team/BLAKE3/archive/refs/tags/
$(package)_file_name=$($(package)_version).tar.gz
$(package)_sha256_hash=fc2aac36643db7e45c3653fd98a2a745e6d4d16ff3711e4b7abd3b88639463dd
$(package)_build_subdir=c/build

define $(package)_set_vars
  $(package)_config_opts=-DCMAKE_BUILD_TYPE=None -DBUILD_SHARED_LIBS=OFF
  $(package)_config_opts+=-DBUILD_TESTING=OFF -DBLAKE3_EXAMPLES=OFF
  $(package)_config_opts+=-DBLAKE3_USE_TBB=OFF -DBLAKE3_SIMD_TYPE=none
endef

define $(package)_config_cmds
  $($(package)_cmake) -S .. -B .
endef

define $(package)_build_cmds
  $(MAKE)
endef

define $(package)_stage_cmds
  $(MAKE) DESTDIR=$($(package)_staging_dir) install
endef
