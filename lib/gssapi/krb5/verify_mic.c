#include "gssapi_locl.h"
#include <des.h>
#include <md5.h>

RCSID("$Id$");

OM_uint32 gss_verify_mic
           (OM_uint32 * minor_status,
            const gss_ctx_id_t context_handle,
            const gss_buffer_t message_buffer,
            const gss_buffer_t token_buffer,
            gss_qop_t * qop_state
	    )
{
  u_char *p;
  size_t len;
  struct md5 md5;
  u_char hash[16];
  des_key_schedule schedule;
  des_cblock key;
  des_cblock zero;

  p = token_buffer->value;
  len = GSS_KRB5_MECHANISM->length + 28;

  if (
      token_buffer->length < len
      || memcmp (p, "\x60\x07\x06\x05", 4) != 0
      || memcmp (p + 4, GSS_KRB5_MECHANISM->elements,
		 GSS_KRB5_MECHANISM->length) != 0)
    return GSS_S_BAD_MECH;
  if (memcmp (p + 4 + GSS_KRB5_MECHANISM->length, "\x01\x01", 2) != 0)
    return GSS_S_DEFECTIVE_TOKEN;
  p += 6 + GSS_KRB5_MECHANISM->length;
  if (memcmp (p, "\x00\x00", 2) != 0)
    return GSS_S_BAD_SIG;
  p += 2;
  if (memcmp (p, "\xff\xff\xff\xff", 4) != 0)
    return GSS_S_BAD_MIC;
  p += 4;
  p += 16;

  md5_init (&md5);
  md5_update (&md5, p - 24, 8);
  md5_update (&md5, message_buffer->value,
	      message_buffer->length);
  md5_finito (&md5, hash);

  memset (&zero, 0, sizeof(zero));
  memcpy (&key, context_handle->auth_context->key.contents.data,
	  sizeof(key));
  des_set_key (&key, schedule);
  des_cbc_cksum ((des_cblock *)hash,
		 (des_cblock *)hash, sizeof(hash), schedule, &zero);
  if (memcmp (p - 8, hash, 8) != 0)
    return GSS_S_BAD_MIC;

  return GSS_S_COMPLETE;
}
