#include "gssapi_locl.h"

RCSID("$Id$");

static krb5_error_code
create_8003_checksum (
		      const gss_channel_bindings_t input_chan_bindings,
		      OM_uint32 flags,
		      Checksum *result)
{
  u_char *p;
  u_int32_t val;

  result->cksumtype = 0x8003;
  result->checksum.length = 24;
  result->checksum.data   = malloc (result->checksum.length);
  if (result->checksum.data == NULL)
    return ENOMEM;
  
  p = result->checksum.data;
  val = 16;
  *p++ = (val >> 0)  & 0xFF;
  *p++ = (val >> 8)  & 0xFF;
  *p++ = (val >> 16) & 0xFF;
  *p++ = (val >> 24) & 0xFF;
  memset (p, 0, 16);
  p += 16;
  val = flags;
  *p++ = (val >> 0)  & 0xFF;
  *p++ = (val >> 8)  & 0xFF;
  *p++ = (val >> 16) & 0xFF;
  *p++ = (val >> 24) & 0xFF;
  if (p - (u_char *)result->checksum.data != result->checksum.length)
    abort ();
  return 0;
}

static OM_uint32
init_auth
           (OM_uint32 * minor_status,
            const gss_cred_id_t initiator_cred_handle,
            gss_ctx_id_t * context_handle,
            const gss_name_t target_name,
            const gss_OID mech_type,
            OM_uint32 req_flags,
            OM_uint32 time_req,
            const gss_channel_bindings_t input_chan_bindings,
            const gss_buffer_t input_token,
            gss_OID * actual_mech_type,
            gss_buffer_t output_token,
            OM_uint32 * ret_flags,
            OM_uint32 * time_rec
           )
{
  OM_uint32 ret;
  krb5_error_code kret;
  krb5_flags ap_options;
  krb5_creds this_cred, *cred;
  krb5_data outbuf;
  krb5_ccache ccache;
  u_int32_t flags;
  Authenticator *auth;
  krb5_data authenticator;
  Checksum cksum;

  gssapi_krb5_init ();

  outbuf.length = 0;
  outbuf.data   = NULL;




  *context_handle = malloc(sizeof(**context_handle));
  if (*context_handle == NULL)
    return GSS_S_FAILURE;

  (*context_handle)->auth_context =  NULL;
  (*context_handle)->source = NULL;
  (*context_handle)->target = NULL;
  (*context_handle)->flags = 0;
  (*context_handle)->more_flags = 0;

  kret = krb5_auth_con_init (gssapi_krb5_context,
			     &(*context_handle)->auth_context);
  if (kret) {
    ret = GSS_S_FAILURE;
    goto failure;
  }

  if (actual_mech_type)
    *actual_mech_type = GSS_KRB5_MECHANISM;

  flags = 0;
  ap_options = 0;
  if (req_flags & GSS_C_DELEG_FLAG)
    ;				/* XXX */
  if (req_flags & GSS_C_MUTUAL_FLAG) {
    flags |= GSS_C_MUTUAL_FLAG;
    ap_options |= AP_OPTS_MUTUAL_REQUIRED;
  }
  if (req_flags & GSS_C_REPLAY_FLAG)
    ;				/* XXX */
  if (req_flags & GSS_C_SEQUENCE_FLAG)
    ;				/* XXX */
  if (req_flags & GSS_C_ANON_FLAG)
    ;				/* XXX */
  flags |= GSS_C_CONF_FLAG;
  flags |= GSS_C_INTEG_FLAG;

  if (ret_flags)
    *ret_flags = flags;
  (*context_handle)->flags = flags;
  (*context_handle)->more_flags = LOCAL;

  kret = krb5_cc_default (gssapi_krb5_context, &ccache);
  if (kret) {
    ret = GSS_S_FAILURE;
    goto failure;
  }

  kret = krb5_cc_get_principal (gssapi_krb5_context,
				ccache,
				&(*context_handle)->source);
  if (kret) {
    ret = GSS_S_FAILURE;
    goto failure;
  }

  kret = krb5_copy_principal (gssapi_krb5_context,
			      target_name,
			      &(*context_handle)->target);
  if (kret) {
    ret = GSS_S_FAILURE;
    goto failure;
  }

  this_cred.client = (*context_handle)->source;
  this_cred.server = (*context_handle)->target;
  this_cred.times.endtime = 0;

  kret = krb5_get_credentials (gssapi_krb5_context,
			       0,
			       ccache,
			       &this_cred,
			       &cred);

  if (kret) {
    ret = GSS_S_FAILURE;
    goto failure;
  }

  (*context_handle)->auth_context->key.keytype = cred->session.keytype;
  krb5_data_copy (&(*context_handle)->auth_context->key.contents,
		  cred->session.contents.data,
		  cred->session.contents.length);

  kret = create_8003_checksum (input_chan_bindings,
			       flags,
			       &cksum);
  if (kret) {
    ret = GSS_S_FAILURE;
    goto failure;
  }

  kret = krb5_build_authenticator (gssapi_krb5_context,
				   (*context_handle)->auth_context,
				   cred,
				   &cksum,
				   &auth,
				   &authenticator);

  if (kret) {
    ret = GSS_S_FAILURE;
    goto failure;
  }

  kret = krb5_build_ap_req (gssapi_krb5_context,
			    cred,
			    ap_options,
			    authenticator,
			    &outbuf);
  krb5_data_free (&authenticator);

  if (kret) {
    ret = GSS_S_FAILURE;
    goto failure;
  }

  ret = gssapi_krb5_encapsulate (&outbuf,
				 output_token,
				 "\x01\x00");
  if (ret)
    goto failure;

  if (flags & GSS_C_MUTUAL_FLAG) {
    return GSS_S_CONTINUE_NEEDED;
  } else {
    (*context_handle)->more_flags |= OPEN;
    return GSS_S_COMPLETE;
  }

failure:
  krb5_auth_con_free (gssapi_krb5_context,
		      (*context_handle)->auth_context);
  if((*context_handle)->source)
    krb5_free_principal (gssapi_krb5_context,
			 (*context_handle)->source);
  if((*context_handle)->target)
    krb5_free_principal (gssapi_krb5_context,
			 (*context_handle)->target);
  free (*context_handle);
  krb5_data_free (&outbuf);
  *context_handle = GSS_C_NO_CONTEXT;
  return GSS_S_FAILURE;
}

static OM_uint32
repl_mutual
           (OM_uint32 * minor_status,
            const gss_cred_id_t initiator_cred_handle,
            gss_ctx_id_t * context_handle,
            const gss_name_t target_name,
            const gss_OID mech_type,
            OM_uint32 req_flags,
            OM_uint32 time_req,
            const gss_channel_bindings_t input_chan_bindings,
            const gss_buffer_t input_token,
            gss_OID * actual_mech_type,
            gss_buffer_t output_token,
            OM_uint32 * ret_flags,
            OM_uint32 * time_rec
           )
{
  OM_uint32 ret;
  krb5_error_code kret;
  krb5_data indata;
  krb5_ap_rep_enc_part *repl;

  ret = gssapi_krb5_decapsulate (input_token,
				 &indata,
				 "\x02\x00");
  if (ret) {
				/* XXX - Handle AP_ERROR */
    return GSS_S_FAILURE;
  }

  kret = krb5_rd_rep (gssapi_krb5_context,
		      (*context_handle)->auth_context,
		      &indata,
		      &repl);
  if (kret)
    return GSS_S_FAILURE;
  krb5_free_ap_rep_enc_part (gssapi_krb5_context,
			     repl);

  (*context_handle)->more_flags |= OPEN;

  return GSS_S_COMPLETE;
}

/*
 * gss_init_sec_context
 */

OM_uint32 gss_init_sec_context
           (OM_uint32 * minor_status,
            const gss_cred_id_t initiator_cred_handle,
            gss_ctx_id_t * context_handle,
            const gss_name_t target_name,
            const gss_OID mech_type,
            OM_uint32 req_flags,
            OM_uint32 time_req,
            const gss_channel_bindings_t input_chan_bindings,
            const gss_buffer_t input_token,
            gss_OID * actual_mech_type,
            gss_buffer_t output_token,
            OM_uint32 * ret_flags,
            OM_uint32 * time_rec
           )
{
  gssapi_krb5_init ();

  if (input_token == GSS_C_NO_BUFFER || input_token->length == 0)
    return init_auth (minor_status,
		      initiator_cred_handle,
		      context_handle,
		      target_name,
		      mech_type,
		      req_flags,
		      time_req,
		      input_chan_bindings,
		      input_token,
		      actual_mech_type,
		      output_token,
		      ret_flags,
		      time_rec);
  else
    return repl_mutual(minor_status,
		       initiator_cred_handle,
		       context_handle,
		       target_name,
		       mech_type,
		       req_flags,
		       time_req,
		       input_chan_bindings,
		       input_token,
		       actual_mech_type,
		       output_token,
		       ret_flags,
		       time_rec);
}
