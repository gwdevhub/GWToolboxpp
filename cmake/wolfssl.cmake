include_guard()

set(wolfssl_FOLDER "${PROJECT_SOURCE_DIR}/Dependencies/wolfssl")

add_library(wolfssl INTERFACE)
target_include_directories(wolfssl INTERFACE "${wolfssl_FOLDER}/include")
target_compile_definitions(wolfssl INTERFACE 
	WC_RSA_BLINDING
	ECC_TIMING_RESISTANT
	TFM_TIMING_RESISTANT
	WC_NO_RNG
	OPENSSL_EXTRA
	NO_OLD_TLS
	NO_MD4
	NO_MD5
	NO_RSA
	NO_RC4
	NO_PWDBASED
	NO_AES_CBC
	NO_DEV_URANDOM
	WOLFSSL_SP_NO_3072
	)
target_link_libraries(wolfssl INTERFACE
    "${wolfssl_FOLDER}/lib/wolfssl.lib")
