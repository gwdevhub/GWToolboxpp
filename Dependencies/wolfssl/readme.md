wolfSSL 5.5.0 compiled with vs2022 with /MT, Whole Program Optimization and /LTCG OFF!

additional lines in user_settings.h:

#define OPENSSL_EXTRA // (this line exists by default)
#define HAVE_SNI
#define WOLFSSL_OPENSSH
#define WOLFSSL_ALLOW_SSLV3

easywsclient must be linked with macros:
OPENSSL_EXTRA
WOLFSSL_ALLOW_SSLV3