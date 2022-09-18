wolfSSL 5.5.0 compiled with vs2022 with /MT, Whole Program Optimization and /LTCG OFF!

additional lines in user_settings.h:

#define OPENSSL_EXTRA // (this line exists by default)
#define HAVE_SNI // wolfSSL_set_tlsext_host_name
#define WOLFSSL_OPENSSH // wolfSSL_set_tlsext_host_name
#define HAVE_SERVER_RENEGOTIATION_INFO // wolfSSL_Rehandshake
#define NO_WOLFSSL_SERVER´// just to cut down size
#define NO_OLD_TLS
#define NO_MD4
#define NO_MD5
#define NO_RSA
#define NO_RC4
#define NO_PWDBASED
#define NO_AES_CBC
#define NO_DEV_URANDOM
#define WOLFSSL_SP_NO_3072

easywsclient must be linked with macros:
OPENSSL_EXTRA