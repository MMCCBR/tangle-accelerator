#include "apis.h"
#include "map/mode.h"

status_t api_get_tips(const iota_client_service_t* const service, char** json_result) {
  status_t ret = SC_OK;
  get_tips_res_t* res = get_tips_res_new();
  char_buffer_t* res_buff = char_buffer_new();

  if (res == NULL || res_buff == NULL) {
    ret = SC_CCLIENT_OOM;
    goto done;
  }

  if (iota_client_get_tips(service, res) != RC_OK) {
    ret = SC_CCLIENT_FAILED_RESPONSE;
    goto done;
  }

  if (service->serializer.vtable.get_tips_serialize_response(res, res_buff) != RC_OK) {
    ret = SC_CCLIENT_JSON_PARSE;
    goto done;
  }

  *json_result = (char*)malloc((res_buff->length + 1) * sizeof(char));
  if (*json_result == NULL) {
    goto done;
  }
  snprintf(*json_result, (res_buff->length + 1), "%s", res_buff->data);

done:
  get_tips_res_free(&res);
  char_buffer_free(res_buff);
  return ret;
}

status_t api_get_tips_pair(const iota_config_t* const tangle, const iota_client_service_t* const service,
                           char** json_result) {
  status_t ret = SC_OK;
  get_transactions_to_approve_req_t* req = get_transactions_to_approve_req_new();
  get_transactions_to_approve_res_t* res = get_transactions_to_approve_res_new();
  char_buffer_t* res_buff = char_buffer_new();

  if (req == NULL || res == NULL || res_buff == NULL) {
    ret = SC_TA_OOM;
    goto done;
  }

  get_transactions_to_approve_req_set_depth(req, tangle->milestone_depth);
  if (iota_client_get_transactions_to_approve(service, req, res) != RC_OK) {
    ret = SC_CCLIENT_FAILED_RESPONSE;
    goto done;
  }

  if (service->serializer.vtable.get_transactions_to_approve_serialize_response(res, res_buff) != RC_OK) {
    ret = SC_CCLIENT_JSON_PARSE;
    goto done;
  }

  *json_result = (char*)malloc((res_buff->length + 1) * sizeof(char));
  if (*json_result == NULL) {
    goto done;
  }
  snprintf(*json_result, (res_buff->length + 1), "%s", res_buff->data);

done:
  get_transactions_to_approve_req_free(&req);
  get_transactions_to_approve_res_free(&res);
  char_buffer_free(res_buff);
  return ret;
}

status_t api_generate_address(const iota_config_t* const tangle, const iota_client_service_t* const service,
                              char** json_result) {
  status_t ret = SC_OK;
  ta_generate_address_res_t* res = ta_generate_address_res_new();
  if (res == NULL) {
    ret = SC_TA_OOM;
    goto done;
  }

  ret = ta_generate_address(tangle, service, res);
  if (ret) {
    goto done;
  }

  ret = ta_generate_address_res_serialize(json_result, res);

done:
  ta_generate_address_res_free(&res);
  return ret;
}

status_t api_get_transaction_object(const iota_client_service_t* const service, const char* const obj,
                                    char** json_result) {
  status_t ret = SC_OK;
  ta_get_transaction_object_res_t* res = ta_get_transaction_object_res_new();
  if (res == NULL) {
    ret = SC_TA_OOM;
    goto done;
  }

  ret = ta_get_transaction_object(service, obj, res);
  if (ret) {
    goto done;
  }

  ret = ta_get_transaction_object_res_serialize(json_result, res);

done:
  ta_get_transaction_object_res_free(&res);
  return ret;
}

status_t api_find_transactions_by_tag(const iota_client_service_t* const service, const char* const obj,
                                      char** json_result) {
  status_t ret = SC_OK;
  ta_find_transactions_res_t* res = ta_find_transactions_res_new();
  if (res == NULL) {
    ret = SC_TA_OOM;
    goto done;
  }

  char* req_tag = (char*)malloc((NUM_TRYTES_TAG + 1) * sizeof(char));
  int obj_len = strlen(obj);

  if (obj_len == NUM_TRYTES_TAG) {
    strncpy(req_tag, obj, NUM_TRYTES_TAG);
  } else if (obj_len < NUM_TRYTES_TAG) {
    fill_nines(req_tag, obj, NUM_TRYTES_TAG);
  } else {  // tag length > 27 trytes
    ret = SC_TA_WRONG_REQUEST_OBJ;
    goto done;
  }

  ret = ta_find_transactions_by_tag(service, req_tag, res);
  if (ret) {
    goto done;
  }

  ret = ta_find_transactions_res_serialize(json_result, res);

done:
  free(req_tag);
  ta_find_transactions_res_free(&res);
  return ret;
}

status_t api_find_transactions_obj_by_tag(const iota_client_service_t* const service, const char* const obj,
                                          char** json_result) {
  status_t ret = SC_OK;
  ta_find_transactions_obj_res_t* res = ta_find_transactions_obj_res_new();
  if (res == NULL) {
    ret = SC_TA_OOM;
    goto done;
  }

  ret = ta_find_transactions_obj_by_tag(service, obj, res);
  if (ret) {
    goto done;
  }

  ret = ta_find_transactions_obj_res_serialize(json_result, res);

done:
  ta_find_transactions_obj_res_free(&res);
  return ret;
}

status_t api_receive_mam_message(const iota_client_service_t* const service, const char* const bundle_hash,
                                 char** json_result) {
  mam_api_t mam;
  status_t ret = SC_OK;
  tryte_t* payload_trytes = NULL;
  tryte_t* none_chid_trytes = NULL;
  char* payload = NULL;
  size_t payload_size = 0;
  bundle_transactions_t* bundle = NULL;
  bundle_transactions_new(&bundle);
  bool is_last_packet;

  // Creating MAM API
  if (mam_api_init(&mam, (tryte_t*)SEED) != RC_OK) {
    ret = SC_MAM_FAILED_INIT;
    goto done;
  }

  ret = ta_get_bundle(service, (tryte_t*)bundle_hash, bundle);
  if (ret != SC_OK) {
    goto done;
  }

  // Set first transaction's address as chid, if no `chid` specified
  iota_transaction_t* curr_tx = (iota_transaction_t*)utarray_eltptr(bundle, 0);
  none_chid_trytes = (tryte_t*)malloc(sizeof(tryte_t) * NUM_TRYTES_ADDRESS);
  flex_trits_to_trytes(none_chid_trytes, NUM_TRYTES_ADDRESS, transaction_address(curr_tx), NUM_TRITS_ADDRESS,
                       NUM_TRITS_ADDRESS);
  mam_api_add_trusted_channel_pk(&mam, none_chid_trytes);

  if (mam_api_bundle_read(&mam, bundle, &payload_trytes, &payload_size, &is_last_packet) == RC_OK) {
    if (payload_trytes == NULL || payload_size == 0) {
      ret = SC_MAM_NO_PAYLOAD;
      goto done;
    } else {
      payload = calloc(payload_size * 2 + 1, sizeof(char));
      if (payload == NULL) {
        ret = SC_TA_NULL;
        goto done;
      }
      trytes_to_ascii(payload_trytes, payload_size, payload);
    }
  } else {
    ret = SC_MAM_NOT_FOUND;
    goto done;
  }

  ret = receive_mam_message_serialize(json_result, &payload);

done:
  // Destroying MAM API
  if (ret != SC_MAM_FAILED_INIT) {
    if (mam_api_destroy(&mam) != RC_OK) {
      ret = SC_MAM_FAILED_DESTROYED;
    }
  }
  free(none_chid_trytes);
  free(payload_trytes);
  free(payload);
  bundle_transactions_free(&bundle);

  return ret;
}

status_t api_mam_send_message(const iota_config_t* const tangle, const iota_client_service_t* const service,
                              char const* const payload, char** json_result) {
  status_t ret = SC_OK;
  mam_api_t mam;
  const bool last_packet = true;
  bundle_transactions_t* bundle = NULL;
  bundle_transactions_new(&bundle);
  tryte_t channel_id[MAM_CHANNEL_ID_TRYTE_SIZE];
  trit_t msg_id[MAM_MSG_ID_SIZE];
  ta_send_mam_req_t* req = send_mam_req_new();
  ta_send_mam_res_t* res = send_mam_res_new();

  // Creating MAM API
  if (mam_api_init(&mam, (tryte_t*)SEED)) {
    ret = SC_MAM_FAILED_INIT;
    goto done;
  }

  ret = send_mam_req_deserialize(payload, req);
  if (ret) {
    goto done;
  }

  // Creating channel
  mam.channel_ord = req->channel_ord;
  if (map_channel_create(&mam, channel_id)) {
    ret = SC_MAM_NULL;
    goto done;
  }

  // Writing header to bundle
  if (map_write_header_on_channel(&mam, channel_id, bundle, msg_id)) {
    ret = SC_MAM_FAILED_WRITE;
    goto done;
  }

  // Writing packet to bundle
  if (map_write_packet(&mam, bundle, req->payload, msg_id, last_packet)) {
    ret = SC_MAM_FAILED_WRITE;
    goto done;
  }
  send_mam_res_set_channel_id(res, channel_id);

  // Sending bundle
  if (ta_send_bundle(tangle, service, bundle) != SC_OK) {
    ret = SC_MAM_FAILED_RESPONSE;
    goto done;
  }
  send_mam_res_set_bundle_hash(res, transaction_bundle((iota_transaction_t*)utarray_front(bundle)));

  ret = send_mam_res_serialize(json_result, res);

done:
  // Destroying MAM API
  if (mam_api_destroy(&mam)) {
    ret = SC_MAM_FAILED_DESTROYED;
  }
  bundle_transactions_free(&bundle);
  send_mam_req_free(&req);
  send_mam_res_free(&res);
  return ret;
}

status_t api_send_transfer(const iota_config_t* const tangle, const iota_client_service_t* const service,
                           const char* const obj, char** json_result) {
  status_t ret = SC_OK;
  char hash_trytes[NUM_TRYTES_HASH + 1];
  ta_send_transfer_req_t* req = ta_send_transfer_req_new();
  ta_send_transfer_res_t* res = ta_send_transfer_res_new();
  ta_get_transaction_object_res_t* txn_obj_res = ta_get_transaction_object_res_new();

  if (req == NULL || res == NULL || txn_obj_res == NULL) {
    ret = SC_TA_OOM;
    goto done;
  }

  ret = ta_send_transfer_req_deserialize(obj, req);
  if (ret) {
    goto done;
  }

  ret = ta_send_transfer(tangle, service, req, res);
  if (ret) {
    goto done;
  }

  // return transaction object
  flex_trits_to_trytes((tryte_t*)hash_trytes, NUM_TRYTES_HASH, hash243_queue_peek(res->hash), NUM_TRITS_HASH,
                       NUM_TRITS_HASH);
  hash_trytes[NUM_TRYTES_HASH] = '\0';
  ret = ta_get_transaction_object(service, hash_trytes, txn_obj_res);
  if (ret) {
    goto done;
  }

  ret = ta_get_transaction_object_res_serialize(json_result, txn_obj_res);

done:
  ta_send_transfer_req_free(&req);
  ta_send_transfer_res_free(&res);
  ta_get_transaction_object_res_free(&txn_obj_res);
  return ret;
}

status_t api_send_trytes(const iota_config_t* const tangle, const iota_client_service_t* const service,
                         const char* const obj, char** json_result) {
  status_t ret = SC_OK;
  hash8019_array_p trytes = hash8019_array_new();

  if (!trytes) {
    ret = SC_TA_OOM;
    goto done;
  }

  ret = ta_send_trytes_req_deserialize(obj, trytes);
  if (ret != SC_OK) {
    goto done;
  }

  ret = ta_send_trytes(tangle, service, trytes);
  if (ret != SC_OK) {
    goto done;
  }

  ret = ta_send_trytes_res_serialize(trytes, json_result);

done:
  hash_array_free(trytes);
  return ret;
}
