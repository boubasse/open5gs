/*
 * Copyright (C) 2019,2020 by Sukchan Lee <acetcom@gmail.com>
 *
 * This file is part of Open5GS.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "test-common.h"

ogs_pkbuf_t *testgmm_build_registration_request(
        test_ue_t *test_ue, bool update)
{
    test_sess_t *sess = NULL;
    uint16_t psimask = 0;

    ogs_nas_5gs_message_t message;
    ogs_pkbuf_t *pkbuf = NULL;
    ogs_nas_5gs_registration_request_t *registration_request =
            &message.gmm.registration_request;
    ogs_nas_5gs_registration_type_t *registration_type =
            &registration_request->registration_type;
    ogs_nas_5gmm_capability_t *gmm_capability =
            &registration_request->gmm_capability;
    ogs_nas_ue_security_capability_t *ue_security_capability =
            &registration_request->ue_security_capability;
    ogs_nas_uplink_data_status_t *uplink_data_status =
            &registration_request->uplink_data_status;
    ogs_nas_5gs_mobile_identity_guti_t mobile_identity_guti;

    ogs_assert(test_ue);
    sess = test_ue->sess;
    ogs_assert(sess);

    memset(&message, 0, sizeof(message));
    if (update) {
        message.h.security_header_type =
            OGS_NAS_SECURITY_HEADER_INTEGRITY_PROTECTED_AND_CIPHERED;
        message.h.extended_protocol_discriminator =
            OGS_NAS_EXTENDED_PROTOCOL_DISCRIMINATOR_5GMM;
    }
    message.gmm.h.extended_protocol_discriminator =
            OGS_NAS_EXTENDED_PROTOCOL_DISCRIMINATOR_5GMM;
    message.gmm.h.message_type = OGS_NAS_5GS_REGISTRATION_REQUEST;

    registration_type->data = test_ue->nas.data;

    if (update) {
        ogs_nas_5gs_nas_guti_to_mobilty_identity_guti(
                &test_ue->nas_guti, &mobile_identity_guti);
        registration_request->mobile_identity.length =
            sizeof(ogs_nas_5gs_mobile_identity_guti_t);
        registration_request->mobile_identity.buffer =
            &mobile_identity_guti;

        registration_request->presencemask |=
            OGS_NAS_5GS_REGISTRATION_REQUEST_UPLINK_DATA_STATUS_PRESENT;
        psimask = 1 << sess->psi;
        uplink_data_status->length = 2;
        uplink_data_status->psi |= psimask << 8;
        uplink_data_status->psi |= psimask >> 8;

    } else {
        registration_request->mobile_identity.length =
            test_ue->mobile_identity_suci_length;
        registration_request->mobile_identity.buffer =
            &test_ue->mobile_identity_suci;
    }

    registration_request->presencemask |=
            OGS_NAS_5GS_REGISTRATION_REQUEST_5GMM_CAPABILITY_PRESENT;
    gmm_capability->length = 1;
    gmm_capability->lte_positioning_protocol_capability = 1;
    gmm_capability->ho_attach = 1; 
    gmm_capability->s1_mode = 1; 

    registration_request->presencemask |=
            OGS_NAS_5GS_REGISTRATION_REQUEST_UE_SECURITY_CAPABILITY_PRESENT;
    ue_security_capability->length = 8;
    ue_security_capability->nea = 0xff;
    ue_security_capability->nia = 0xff;
    ue_security_capability->eps_ea = 0xff;
    ue_security_capability->eps_ia = 0xff;

    if (update)
        return test_nas_5gs_security_encode(test_ue, &message);
    else
        return ogs_nas_5gs_plain_encode(&message);
}

ogs_pkbuf_t *testgmm_build_identity_response(test_ue_t *test_ue)
{
    ogs_nas_5gs_message_t message;
    ogs_pkbuf_t *pkbuf = NULL;
    ogs_nas_5gs_identity_response_t *identity_response =
            &message.gmm.identity_response;

    ogs_assert(test_ue);

    memset(&message, 0, sizeof(message));
    message.gmm.h.extended_protocol_discriminator =
            OGS_NAS_EXTENDED_PROTOCOL_DISCRIMINATOR_5GMM;
    message.gmm.h.message_type = OGS_NAS_5GS_IDENTITY_RESPONSE;

    identity_response->mobile_identity.length =
        test_ue->mobile_identity_suci_length;
    identity_response->mobile_identity.buffer =
        &test_ue->mobile_identity_suci;

    return ogs_nas_5gs_plain_encode(&message);
}

ogs_pkbuf_t *testgmm_build_authentication_response(test_ue_t *test_ue)
{
    ogs_nas_5gs_message_t message;
    ogs_pkbuf_t *pkbuf = NULL;
    ogs_nas_5gs_authentication_response_t *authentication_response =
            &message.gmm.authentication_response;
    ogs_nas_authentication_response_parameter_t
        *authentication_response_parameter =
            &authentication_response->authentication_response_parameter;

    uint8_t ik[OGS_KEY_LEN];
    uint8_t ck[OGS_KEY_LEN];
    uint8_t ak[OGS_AK_LEN];
    uint8_t res[OGS_MAX_RES_LEN];
    uint8_t res_star[OGS_MAX_RES_LEN];
    uint8_t kausf[OGS_SHA256_DIGEST_SIZE];
    uint8_t kseaf[OGS_SHA256_DIGEST_SIZE];
    char *serving_network_name;

    ogs_assert(test_ue);

    memset(&message, 0, sizeof(message));
    message.gmm.h.extended_protocol_discriminator =
            OGS_NAS_EXTENDED_PROTOCOL_DISCRIMINATOR_5GMM;
    message.gmm.h.message_type = OGS_NAS_5GS_AUTHENTICATION_RESPONSE;

    authentication_response->presencemask |=
        OGS_NAS_5GS_AUTHENTICATION_RESPONSE_AUTHENTICATION_RESPONSE_PARAMETER_PRESENT;

    milenage_f2345(test_ue->opc, test_ue->k, test_ue->rand,
            res, ck, ik, ak, NULL);
    serving_network_name =
        ogs_serving_network_name_from_plmn_id(&test_self()->tai.plmn_id);
    ogs_kdf_xres_star(
            ck, ik,
            serving_network_name, test_ue->rand, res, 8,
            authentication_response_parameter->res);

    authentication_response_parameter->length = OGS_AUTN_LEN;

    memcpy(res_star, authentication_response_parameter->res,
            authentication_response_parameter->length);
    ogs_kdf_kausf(ck, ik, serving_network_name, test_ue->autn, kausf);
    ogs_kdf_kseaf(serving_network_name, kausf, kseaf);
    ogs_kdf_kamf(test_ue->supi, test_ue->abba, test_ue->abba_len,
                kseaf, test_ue->kamf);

    ogs_free(serving_network_name);

    return ogs_nas_5gs_plain_encode(&message);
}

ogs_pkbuf_t *testgmm_build_authentication_failure(
        test_ue_t *test_ue, ogs_nas_5gmm_cause_t gmm_cause)

{
    ogs_nas_5gs_message_t message;
    ogs_pkbuf_t *pkbuf = NULL;
    ogs_nas_5gs_authentication_failure_t *authentication_failure =
            &message.gmm.authentication_failure;
    ogs_nas_authentication_failure_parameter_t
        *authentication_failure_parameter =
            &authentication_failure->authentication_failure_parameter;

    uint8_t ak[OGS_AK_LEN];
    uint8_t sqn_ms[OGS_SQN_LEN] = "\x00\x00\x11\x22\x33\x44";
    uint8_t mac_s[OGS_MAC_S_LEN];
    uint8_t amf[2] = { 0, 0 };
    uint8_t auts[OGS_AUTS_LEN];
    int i = 0;

    ogs_assert(test_ue);

    memset(&message, 0, sizeof(message));
    message.gmm.h.extended_protocol_discriminator =
            OGS_NAS_EXTENDED_PROTOCOL_DISCRIMINATOR_5GMM;
    message.gmm.h.message_type = OGS_NAS_5GS_AUTHENTICATION_FAILURE;

    authentication_failure->gmm_cause = gmm_cause;

    if (gmm_cause == OGS_5GMM_CAUSE_SYNCH_FAILURE) {
        authentication_failure->presencemask |=
            OGS_NAS_5GS_AUTHENTICATION_RESPONSE_AUTHENTICATION_RESPONSE_PARAMETER_PRESENT;

        milenage_f2345(test_ue->opc, test_ue->k, test_ue->rand,
                NULL, NULL, NULL, NULL, ak);

        milenage_f1(test_ue->opc, test_ue->k, test_ue->rand,
                sqn_ms, amf, NULL, auts + OGS_SQN_LEN);
        for (i = 0; i < OGS_SQN_LEN; i++)
            auts[i] = sqn_ms[i] ^ ak[i];

        authentication_failure_parameter->length = OGS_AUTS_LEN;
        memcpy(authentication_failure_parameter->auts, auts,
                        authentication_failure_parameter->length);
    }

    return ogs_nas_5gs_plain_encode(&message);
}

ogs_pkbuf_t *testgmm_build_security_mode_complete(
        test_ue_t *test_ue, ogs_pkbuf_t *nasbuf)
{
    ogs_nas_5gs_message_t message;
    ogs_pkbuf_t *pkbuf = NULL;
    ogs_nas_5gs_security_mode_complete_t *security_mode_complete =
            &message.gmm.security_mode_complete;
    ogs_nas_message_container_t *nas_message_container =
            &security_mode_complete->nas_message_container;
    ogs_nas_5gs_mobile_identity_t *imeisv =
            &security_mode_complete->imeisv;

    ogs_nas_mobile_identity_imeisv_t mobile_identity_imeisv;

    ogs_assert(test_ue);

    memset(&message, 0, sizeof(message));
    message.h.security_header_type =
        OGS_NAS_SECURITY_HEADER_INTEGRITY_PROTECTED_AND_CIPHTERD_WITH_NEW_INTEGRITY_CONTEXT;
    message.h.extended_protocol_discriminator =
        OGS_NAS_EXTENDED_PROTOCOL_DISCRIMINATOR_5GMM;

    message.gmm.h.extended_protocol_discriminator =
        OGS_NAS_EXTENDED_PROTOCOL_DISCRIMINATOR_5GMM;
    message.gmm.h.message_type = OGS_NAS_5GS_SECURITY_MODE_COMPLETE;

    security_mode_complete->presencemask |=
        OGS_NAS_5GS_SECURITY_MODE_COMPLETE_IMEISV_PRESENT;
    memset(&mobile_identity_imeisv, 0, sizeof(mobile_identity_imeisv));
    imeisv->length = sizeof(mobile_identity_imeisv);
    imeisv->buffer = &mobile_identity_imeisv;

    mobile_identity_imeisv.type = OGS_NAS_5GS_MOBILE_IDENTITY_IMEISV;
    mobile_identity_imeisv.odd_even = OGS_NAS_MOBILE_IDENTITY_EVEN;
    mobile_identity_imeisv.digit1 = 1;
    mobile_identity_imeisv.digit2 = 1;
    mobile_identity_imeisv.digit3 = 1;

    if (nasbuf) {
        security_mode_complete->presencemask |=
            OGS_NAS_5GS_SECURITY_MODE_COMPLETE_NAS_MESSAGE_CONTAINER_PRESENT;
        nas_message_container->length = nasbuf->len;
        nas_message_container->buffer = nasbuf->data;
        ogs_pkbuf_free(nasbuf);
    }

    return test_nas_5gs_security_encode(test_ue, &message);
}

ogs_pkbuf_t *testgmm_build_registration_complete(test_ue_t *test_ue)
{
    ogs_nas_5gs_message_t message;
    ogs_pkbuf_t *pkbuf = NULL;
    ogs_nas_5gs_registration_complete_t *registration_complete =
            &message.gmm.registration_complete;

    ogs_assert(test_ue);

    memset(&message, 0, sizeof(message));
    message.h.security_header_type =
        OGS_NAS_SECURITY_HEADER_INTEGRITY_PROTECTED_AND_CIPHERED;
    message.h.extended_protocol_discriminator =
        OGS_NAS_EXTENDED_PROTOCOL_DISCRIMINATOR_5GMM;

    message.gmm.h.extended_protocol_discriminator =
        OGS_NAS_EXTENDED_PROTOCOL_DISCRIMINATOR_5GMM;
    message.gmm.h.message_type = OGS_NAS_5GS_REGISTRATION_COMPLETE;

    return test_nas_5gs_security_encode(test_ue, &message);
}

ogs_pkbuf_t *testgmm_build_service_request(test_ue_t *test_ue)
{
    ogs_nas_5gs_message_t message;
    ogs_pkbuf_t *pkbuf = NULL;
    ogs_nas_5gs_service_request_t *service_request =
            &message.gmm.service_request;
    ogs_nas_5gs_mobile_identity_s_tmsi_t mobile_identity_s_tmsi;
    ogs_nas_uplink_data_status_t *uplink_data_status = NULL;

    test_sess_t *sess = NULL;
    uint16_t psimask = 0;

    ogs_assert(test_ue);
    sess = test_ue->sess;
    ogs_assert(sess);

    uplink_data_status = &service_request->uplink_data_status;

    memset(&message, 0, sizeof(message));
    message.h.security_header_type =
        OGS_NAS_SECURITY_HEADER_INTEGRITY_PROTECTED_AND_CIPHERED;
    message.h.extended_protocol_discriminator =
        OGS_NAS_EXTENDED_PROTOCOL_DISCRIMINATOR_5GMM;

    message.gmm.h.extended_protocol_discriminator =
        OGS_NAS_EXTENDED_PROTOCOL_DISCRIMINATOR_5GMM;
    message.gmm.h.message_type = OGS_NAS_5GS_SERVICE_REQUEST;

    service_request->ngksi.type = OGS_NAS_SERVICE_TYPE_DATA;
    service_request->ngksi.tsc = test_ue->nas.tsc;
    service_request->ngksi.value = test_ue->nas.ksi;

    ogs_assert(test_ue->nas_guti.m_tmsi);
    memset(&mobile_identity_s_tmsi, 0, sizeof(mobile_identity_s_tmsi));

    mobile_identity_s_tmsi.h.type = OGS_NAS_5GS_MOBILE_IDENTITY_S_TMSI;
    mobile_identity_s_tmsi.m_tmsi = htobe32(test_ue->nas_guti.m_tmsi);
    mobile_identity_s_tmsi.set1 = test_ue->nas_guti.amf_id.set1;
    mobile_identity_s_tmsi.set2 = test_ue->nas_guti.amf_id.set2;
    mobile_identity_s_tmsi.pointer = test_ue->nas_guti.amf_id.pointer;
    service_request->s_tmsi.length = sizeof(mobile_identity_s_tmsi);
    service_request->s_tmsi.buffer = &mobile_identity_s_tmsi;

    service_request->presencemask |=
        OGS_NAS_5GS_SERVICE_REQUEST_UPLINK_DATA_STATUS_PRESENT;
    psimask = 1 << sess->psi;
    uplink_data_status->length = 2;
    uplink_data_status->psi |= psimask << 8;
    uplink_data_status->psi |= psimask >> 8;

    return test_nas_5gs_security_encode(test_ue, &message);
}

ogs_pkbuf_t *testgmm_build_configuration_update_complete(test_ue_t *test_ue)
{
    ogs_nas_5gs_message_t message;
    ogs_pkbuf_t *pkbuf = NULL;
    ogs_nas_5gs_configuration_update_complete_t *configuration_update_complete =
            &message.gmm.configuration_update_complete;
    ogs_nas_control_plane_service_type_t *control_plane_service_type = NULL;

    ogs_assert(test_ue);

    control_plane_service_type =
        &configuration_update_complete->control_plane_service_type;

    memset(&message, 0, sizeof(message));
    message.h.security_header_type =
        OGS_NAS_SECURITY_HEADER_INTEGRITY_PROTECTED_AND_CIPHERED;
    message.h.extended_protocol_discriminator =
        OGS_NAS_EXTENDED_PROTOCOL_DISCRIMINATOR_5GMM;

    message.gmm.h.extended_protocol_discriminator =
        OGS_NAS_EXTENDED_PROTOCOL_DISCRIMINATOR_5GMM;
    message.gmm.h.message_type = OGS_NAS_5GS_CONFIGURATION_UPDATE_COMPLETE;

    control_plane_service_type->value =
        OGS_NAS_5GS_CONTROL_PLANE_SERVICE_TYPE_MO;

    return test_nas_5gs_security_encode(test_ue, &message);
}

ogs_pkbuf_t *testgmm_build_ul_nas_transport(test_sess_t *test_sess,
        uint8_t payload_type, ogs_pkbuf_t *payload)
{
    test_ue_t *test_ue = NULL;
    ogs_pkbuf_t *pkbuf = NULL;

    ogs_nas_5gs_message_t message;
    ogs_nas_5gs_ul_nas_transport_t *ul_nas_transport =
            &message.gmm.ul_nas_transport;

    ogs_nas_payload_container_type_t *payload_container_type = NULL;
    ogs_nas_payload_container_t *payload_container = NULL;
    ogs_nas_pdu_session_identity_2_t *pdu_session_id = NULL;
    ogs_nas_request_type_t *request_type = NULL;
    ogs_nas_s_nssai_t *s_nssai = NULL;

    ogs_assert(test_sess);
    test_ue = test_sess->test_ue;
    ogs_assert(test_ue);
    ogs_assert(payload_type);
    ogs_assert(payload);

    payload_container_type = &ul_nas_transport->payload_container_type;
    payload_container = &ul_nas_transport->payload_container;
    pdu_session_id = &ul_nas_transport->pdu_session_id;
    request_type = &ul_nas_transport->request_type;
    s_nssai = &ul_nas_transport->s_nssai;

    memset(&message, 0, sizeof(message));
    message.h.security_header_type =
        OGS_NAS_SECURITY_HEADER_INTEGRITY_PROTECTED_AND_CIPHERED;
    message.h.extended_protocol_discriminator =
        OGS_NAS_EXTENDED_PROTOCOL_DISCRIMINATOR_5GMM;

    message.gmm.h.extended_protocol_discriminator =
        OGS_NAS_EXTENDED_PROTOCOL_DISCRIMINATOR_5GMM;
    message.gmm.h.message_type = OGS_NAS_5GS_UL_NAS_TRANSPORT;

    payload_container_type->value = payload_type;

    payload_container->length = payload->len;
    payload_container->buffer = payload->data;

    ul_nas_transport->presencemask |=
            OGS_NAS_5GS_UL_NAS_TRANSPORT_PDU_SESSION_ID_PRESENT;
    *pdu_session_id = test_sess->psi;

    ul_nas_transport->presencemask |=
            OGS_NAS_5GS_UL_NAS_TRANSPORT_REQUEST_TYPE_PRESENT;
    request_type->value = OGS_NAS_5GS_REQUEST_TYPE_INITIAL;

    ul_nas_transport->presencemask |=
            OGS_NAS_5GS_UL_NAS_TRANSPORT_S_NSSAI_PRESENT;
    s_nssai->length = 4;
    s_nssai->sst = test_self()->plmn_support[0].s_nssai[0].sst;
    s_nssai->sd = ogs_htobe24(test_self()->plmn_support[0].s_nssai[0].sd);

    ul_nas_transport->presencemask |=
            OGS_NAS_5GS_UL_NAS_TRANSPORT_DNN_PRESENT;
    ul_nas_transport->dnn.length = strlen(test_sess->dnn);
    ogs_cpystrn(ul_nas_transport->dnn.value, test_sess->dnn,
            ogs_min(ul_nas_transport->dnn.length, OGS_MAX_DNN_LEN) + 1);

    pkbuf = test_nas_5gs_security_encode(test_ue, &message);
    ogs_pkbuf_free(payload);

    return pkbuf;
}

ogs_pkbuf_t *testgmm_build_de_registration_request(
        test_ue_t *test_ue, bool switch_off)
{
    ogs_nas_5gs_message_t message;
    ogs_pkbuf_t *pkbuf = NULL;
    ogs_nas_5gs_deregistration_request_from_ue_t
        *deregistration_request_from_ue =
            &message.gmm.deregistration_request_from_ue;
    ogs_nas_de_registration_type_t *de_registration_type =
            &deregistration_request_from_ue->de_registration_type;
    ogs_nas_5gs_mobile_identity_guti_t mobile_identity_guti;

    ogs_assert(test_ue);

    memset(&message, 0, sizeof(message));
    message.h.security_header_type =
        OGS_NAS_SECURITY_HEADER_INTEGRITY_PROTECTED_AND_CIPHERED;
    message.h.extended_protocol_discriminator =
        OGS_NAS_EXTENDED_PROTOCOL_DISCRIMINATOR_5GMM;

    message.gmm.h.extended_protocol_discriminator =
        OGS_NAS_EXTENDED_PROTOCOL_DISCRIMINATOR_5GMM;
    message.gmm.h.message_type = OGS_NAS_5GS_DEREGISTRATION_REQUEST;

    de_registration_type->ksi = test_ue->nas.ksi;
    de_registration_type->switch_off = switch_off;
    de_registration_type->access_type = test_ue->nas.access_type;

    if (test_ue->nas_guti.m_tmsi) {
        ogs_nas_5gs_nas_guti_to_mobilty_identity_guti(
                &test_ue->nas_guti, &mobile_identity_guti);
        deregistration_request_from_ue->mobile_identity.length =
            sizeof(ogs_nas_5gs_mobile_identity_guti_t);
        deregistration_request_from_ue->mobile_identity.buffer =
            &mobile_identity_guti;
    } else {
        deregistration_request_from_ue->mobile_identity.length =
            test_ue->mobile_identity_suci_length;
        deregistration_request_from_ue->mobile_identity.buffer =
            &test_ue->mobile_identity_suci;
    }

    return test_nas_5gs_security_encode(test_ue, &message);
}

ogs_pkbuf_t *testgmm_build_gmm_status(
        test_ue_t *test_ue, ogs_nas_5gmm_cause_t gmm_cause)
{
    ogs_nas_5gs_message_t message;
    ogs_pkbuf_t *pkbuf = NULL;
    ogs_nas_5gs_5gmm_status_t *gmm_status = &message.gmm.gmm_status;

    ogs_assert(test_ue);
    ogs_assert(gmm_status);

    memset(&message, 0, sizeof(message));
    message.gmm.h.extended_protocol_discriminator =
            OGS_NAS_EXTENDED_PROTOCOL_DISCRIMINATOR_5GMM;
    message.gmm.h.message_type = OGS_NAS_5GS_5GMM_STATUS;

    gmm_status->gmm_cause = gmm_cause;

    return ogs_nas_5gs_plain_encode(&message);
}