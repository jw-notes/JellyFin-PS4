// JellyFin [PS] for PS4 - Copyright (C) 2026 jw-notes <https://github.com/jw-notes>
// GPL-3.0-or-later, with the attribution terms in NOTICE.
//
// See session_crypt.h. The key is SHA-256 of a fixed label and the console's
// OpenPSID, so it is the same for every user of the console (Share Sign-in
// reads one user's sealed token in another's session) and differs on every
// other console. Each seal has its own 96-bit nonce, made from the clock, a
// counter and the last nonce: never repeated under one key.
#include "session_crypt.h"
#include "mono_clock.h"
#include "app.h"
#include <mbedtls/base64.h>
#include <mbedtls/gcm.h>
#include <mbedtls/platform_util.h>
#include <mbedtls/sha256.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <vector>

extern "C" int32_t sceKernelGetOpenPsIdForSystem(void* id);   // libkernel: the console's 16-byte OpenPSID

static const char LABEL[]="JellyFin-PS4 sign-in v1";
static const unsigned char AAD[]="jfps4-token";
static constexpr size_t NONCE=12,TAG=16;

static bool console_key(unsigned char key[32]){
 unsigned char id[16]={};
 const int32_t rc=sceKernelGetOpenPsIdForSystem(id);
 bool blank=true;for(unsigned char b:id)if(b)blank=false;
 if(rc!=0||blank){char msg[96];snprintf(msg,sizeof msg,"Account: the console's ID could not be read (0x%08x); the sign-in is not sealed",(unsigned)rc);plog(msg);return false;}
 mbedtls_sha256_context sha;mbedtls_sha256_init(&sha);
 mbedtls_sha256_starts_ret(&sha,0);mbedtls_sha256_update_ret(&sha,(const unsigned char*)LABEL,sizeof LABEL-1);mbedtls_sha256_update_ret(&sha,id,sizeof id);mbedtls_sha256_finish_ret(&sha,key);
 mbedtls_sha256_free(&sha);mbedtls_platform_zeroize(id,sizeof id);
 return true;
}
static void make_nonce(unsigned char nonce[NONCE]){
 static unsigned char last[32]={};static uint64_t counter=0;
 struct { uint64_t us,count; time_t now; const void* where; } seed{mono_us(),++counter,time(nullptr),&seed};
 mbedtls_sha256_context sha;mbedtls_sha256_init(&sha);
 mbedtls_sha256_starts_ret(&sha,0);mbedtls_sha256_update_ret(&sha,last,sizeof last);mbedtls_sha256_update_ret(&sha,(const unsigned char*)&seed,sizeof seed);mbedtls_sha256_finish_ret(&sha,last);
 mbedtls_sha256_free(&sha);memcpy(nonce,last,NONCE);
}

std::string seal_token(const std::string& token){
 unsigned char key[32];if(token.empty()||!console_key(key))return {};
 std::vector<unsigned char> blob(NONCE+token.size()+TAG);
 make_nonce(blob.data());
 mbedtls_gcm_context gcm;mbedtls_gcm_init(&gcm);
 bool ok=mbedtls_gcm_setkey(&gcm,MBEDTLS_CIPHER_ID_AES,key,256)==0&&
  mbedtls_gcm_crypt_and_tag(&gcm,MBEDTLS_GCM_ENCRYPT,token.size(),blob.data(),NONCE,AAD,sizeof AAD-1,(const unsigned char*)token.data(),blob.data()+NONCE,TAG,blob.data()+NONCE+token.size())==0;
 mbedtls_gcm_free(&gcm);mbedtls_platform_zeroize(key,sizeof key);
 if(!ok)return {};
 size_t len=0;mbedtls_base64_encode(nullptr,0,&len,blob.data(),blob.size());
 std::string out(len,'\0');
 if(mbedtls_base64_encode((unsigned char*)&out[0],out.size(),&len,blob.data(),blob.size())!=0)return {};
 out.resize(len);return "v1:"+out;
}

bool open_token(const std::string& sealed,std::string& token){
 token.clear();if(sealed.rfind("v1:",0)!=0)return false;
 const std::string text=sealed.substr(3);
 size_t len=0;mbedtls_base64_decode(nullptr,0,&len,(const unsigned char*)text.data(),text.size());
 std::vector<unsigned char> blob(len);
 if(len<NONCE+TAG+1||mbedtls_base64_decode(blob.data(),blob.size(),&len,(const unsigned char*)text.data(),text.size())!=0)return false;
 blob.resize(len);
 unsigned char key[32];if(!console_key(key))return false;
 const size_t n=blob.size()-NONCE-TAG;std::vector<unsigned char> plain(n);
 mbedtls_gcm_context gcm;mbedtls_gcm_init(&gcm);
 const bool ok=mbedtls_gcm_setkey(&gcm,MBEDTLS_CIPHER_ID_AES,key,256)==0&&
  mbedtls_gcm_auth_decrypt(&gcm,n,blob.data(),NONCE,AAD,sizeof AAD-1,blob.data()+NONCE+n,TAG,blob.data()+NONCE,plain.data())==0;
 mbedtls_gcm_free(&gcm);mbedtls_platform_zeroize(key,sizeof key);
 if(ok)token.assign((const char*)plain.data(),n);
 mbedtls_platform_zeroize(plain.data(),plain.size());
 return ok;
}
