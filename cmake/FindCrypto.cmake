# Kill it xd
set(CMAKE_MODULE_PATH ${ORIGINAL_CMAKE_MODULE_PATH})
find_package(OpenSSL)
set(CMAKE_MODULE_PATH ${OWN_CMAKE_MODULE_PATH})

set(OPENSSL_CRYPTO_LIBRARIES)
set(OPENSSL_INCLUDE_DIRS)
if(OPENSSL_FOUND)
  set(OPENSSL_CRYPTO_LIBRARIES ${OPENSSL_CRYPTO_LIBRARY})
  set(OPENSSL_INCLUDE_DIRS ${OPENSSL_INCLUDE_DIR})
endif()
