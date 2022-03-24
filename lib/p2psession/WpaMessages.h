/* wpa_supplicant control interface - fixed message prefixes */
#pragma once

#include <string>

namespace p2psession {



enum class WpaEventMessage {
    WPA_UNKOWN_MESSAGE,         // "UNKNOWN"
/** Interactive request for identity/password/pin */
   WPA_CTRL_REQ,         // "CTRL-REQ-"

/** Response to identity/password/pin request */
    WPA_CTRL_RSP,         // ""CTRL-RSP-"

/* Event messages with fixed prefix */
/** Authentication completed successfully and data connection enabled */
   WPA_EVENT_CONNECTED,         // "CTRL-EVENT-CONNECTED "
/** Disconnected, data connection is not available */
   WPA_EVENT_DISCONNECTED,         // "CTRL-EVENT-DISCONNECTED "
/** Association rejected during connection attempt */
   WPA_EVENT_ASSOC_REJECT,         // "CTRL-EVENT-ASSOC-REJECT "
/** Authentication rejected during connection attempt */
   WPA_EVENT_AUTH_REJECT,         // "CTRL-EVENT-AUTH-REJECT "
/** wpa_supplicant is exiting */
   WPA_EVENT_TERMINATING,         // "CTRL-EVENT-TERMINATING "
/** Password change was completed successfully */
   WPA_EVENT_PASSWORD_CHANGED,         // "CTRL-EVENT-PASSWORD-CHANGED "
/** EAP-Request/Notification received */
   WPA_EVENT_EAP_NOTIFICATION,         // "CTRL-EVENT-EAP-NOTIFICATION "
/** EAP authentication started (EAP-Request/Identity received) */
   WPA_EVENT_EAP_STARTED,         // "CTRL-EVENT-EAP-STARTED "
/** EAP method proposed by the server */
   WPA_EVENT_EAP_PROPOSED_METHOD,         // "CTRL-EVENT-EAP-PROPOSED-METHOD "
/** EAP method selected */
   WPA_EVENT_EAP_METHOD,         // "CTRL-EVENT-EAP-METHOD "
/** EAP peer certificate from TLS */
   WPA_EVENT_EAP_PEER_CERT,         // "CTRL-EVENT-EAP-PEER-CERT "
/** EAP peer certificate alternative subject name component from TLS */
   WPA_EVENT_EAP_PEER_ALT,         // "CTRL-EVENT-EAP-PEER-ALT "
/** EAP TLS certificate chain validation error */
   WPA_EVENT_EAP_TLS_CERT_ERROR,         // "CTRL-EVENT-EAP-TLS-CERT-ERROR "
/** EAP status */
   WPA_EVENT_EAP_STATUS,         // "CTRL-EVENT-EAP-STATUS "
/** Retransmit the previous request packet */
   WPA_EVENT_EAP_RETRANSMIT,         // "CTRL-EVENT-EAP-RETRANSMIT "
    WPA_EVENT_EAP_RETRANSMIT2,         // "CTRL-EVENT-EAP-RETRANSMIT2 "
/** EAP authentication completed successfully */
   WPA_EVENT_EAP_SUCCESS,         // "CTRL-EVENT-EAP-SUCCESS "
    WPA_EVENT_EAP_SUCCESS2,         // "CTRL-EVENT-EAP-SUCCESS2 "
/** EAP authentication failed (EAP-Failure received) */
   WPA_EVENT_EAP_FAILURE,         // "CTRL-EVENT-EAP-FAILURE "
    WPA_EVENT_EAP_FAILURE2,         // "CTRL-EVENT-EAP-FAILURE2 "
/** EAP authentication failed due to no response received */
   WPA_EVENT_EAP_TIMEOUT_FAILURE,         // "CTRL-EVENT-EAP-TIMEOUT-FAILURE "
    WPA_EVENT_EAP_TIMEOUT_FAILURE2,         // "CTRL-EVENT-EAP-TIMEOUT-FAILURE2 "
   WPA_EVENT_EAP_ERROR_CODE,         // "EAP-ERROR-CODE "
/** Network block temporarily disabled (e.g., due to authentication failure) */
   WPA_EVENT_TEMP_DISABLED,         // "CTRL-EVENT-SSID-TEMP-DISABLED "
/** Temporarily disabled network block re-enabled */
   WPA_EVENT_REENABLED,         // "CTRL-EVENT-SSID-REENABLED "
/** New scan started */
   WPA_EVENT_SCAN_STARTED,         // "CTRL-EVENT-SCAN-STARTED "
/** New scan results available */
   WPA_EVENT_SCAN_RESULTS,         // "CTRL-EVENT-SCAN-RESULTS "
/** Scan command failed */
   WPA_EVENT_SCAN_FAILED,         // "CTRL-EVENT-SCAN-FAILED "
/** wpa_supplicant state change */
   WPA_EVENT_STATE_CHANGE,         // "CTRL-EVENT-STATE-CHANGE "
/** A new BSS entry was added (followed by BSS entry id and BSSID) */
   WPA_EVENT_BSS_ADDED,         // "CTRL-EVENT-BSS-ADDED "
/** A BSS entry was removed (followed by BSS entry id and BSSID) */
   WPA_EVENT_BSS_REMOVED,         // "CTRL-EVENT-BSS-REMOVED "
/** No suitable network was found */
   WPA_EVENT_NETWORK_NOT_FOUND,         // "CTRL-EVENT-NETWORK-NOT-FOUND "
/** Change in the signal level was reported by the driver */
   WPA_EVENT_SIGNAL_CHANGE,         // "CTRL-EVENT-SIGNAL-CHANGE "
/** Beacon loss reported by the driver */
   WPA_EVENT_BEACON_LOSS,         // "CTRL-EVENT-BEACON-LOSS "
/** Regulatory domain channel */
   WPA_EVENT_REGDOM_CHANGE,         // "CTRL-EVENT-REGDOM-CHANGE "
/** Channel switch started (followed by freq=<MHz> and other channel parameters)
 */
   WPA_EVENT_CHANNEL_SWITCH_STARTED,         // "CTRL-EVENT-STARTED-CHANNEL-SWITCH "

/** Channel switch (followed by freq=<MHz> and other channel parameters) */
   WPA_EVENT_CHANNEL_SWITCH,         // "CTRL-EVENT-CHANNEL-SWITCH "
/** SAE authentication failed due to unknown password identifier */
    WPA_EVENT_SAE_UNKNOWN_PASSWORD_IDENTIFIER, // "CTRL-EVENT-SAE-UNKNOWN-PASSWORD-IDENTIFIER "
/** Unprotected Beacon frame dropped */
   WPA_EVENT_UNPROT_BEACON,         // "CTRL-EVENT-UNPROT-BEACON "
/** Decision made to do a within-ESS roam */
   WPA_EVENT_DO_ROAM,         // "CTRL-EVENT-DO-ROAM "
/** Decision made to skip a within-ESS roam */
   WPA_EVENT_SKIP_ROAM,         // "CTRL-EVENT-SKIP-ROAM "

/** IP subnet status change notification
 *
 * When using an offloaded roaming mechanism where driver/firmware takes care
 * of roaming and IP subnet validation checks post-roaming, this event can
 * indicate whether IP subnet has changed.
 *
 * The event has a status=<0/1/2> parameter where
 * 0 = unknown
 * 1 = IP subnet unchanged (can continue to use the old IP address)
 * 2 = IP subnet changed (need to get a new IP address)
 */
   WPA_EVENT_SUBNET_STATUS_UPDATE,         // "CTRL-EVENT-SUBNET-STATUS-UPDATE "

/** RSN IBSS 4-way handshakes completed with specified peer */
   IBSS_RSN_COMPLETED,         // "IBSS-RSN-COMPLETED "

/** Notification of frequency conflict due to a concurrent operation.
 *
 * The indicated network is disabled and needs to be re-enabled before it can
 * be used again.
 */
   WPA_EVENT_FREQ_CONFLICT,         // "CTRL-EVENT-FREQ-CONFLICT "
/** Frequency ranges that the driver recommends to avoid */
   WPA_EVENT_AVOID_FREQ,         // "CTRL-EVENT-AVOID-FREQ "
/** A new network profile was added (followed by network entry id) */
   WPA_EVENT_NETWORK_ADDED,         // "CTRL-EVENT-NETWORK-ADDED "
/** A network profile was removed (followed by prior network entry id) */
   WPA_EVENT_NETWORK_REMOVED,         // "CTRL-EVENT-NETWORK-REMOVED "
/** Result of MSCS setup */
   WPA_EVENT_MSCS_RESULT,         // "CTRL-EVENT-MSCS-RESULT "
/** WPS overlap detected in PBC mode */
   WPS_EVENT_OVERLAP,         // "WPS-OVERLAP-DETECTED "
/** Available WPS AP with active PBC found in scan results */
   WPS_EVENT_AP_AVAILABLE_PBC,         // "WPS-AP-AVAILABLE-PBC "
/** Available WPS AP with our address as authorized in scan results */
   WPS_EVENT_AP_AVAILABLE_AUTH,         // "WPS-AP-AVAILABLE-AUTH "
/** Available WPS AP with recently selected PIN registrar found in scan results
 */
   WPS_EVENT_AP_AVAILABLE_PIN,         // "WPS-AP-AVAILABLE-PIN "
/** Available WPS AP found in scan results */
   WPS_EVENT_AP_AVAILABLE,         // "WPS-AP-AVAILABLE "
/** A new credential received */
   WPS_EVENT_CRED_RECEIVED,         // "WPS-CRED-RECEIVED "
/** M2D received */
    WPS_EVENT_M2D,         /// "WPS-M2D "
/** WPS registration failed after M2/M2D */
   WPS_EVENT_FAIL,         // "WPS-FAIL "
/** WPS registration completed successfully */
   WPS_EVENT_SUCCESS,         // "WPS-SUCCESS "
/** WPS enrollment attempt timed out and was terminated */
   WPS_EVENT_TIMEOUT,         // "WPS-TIMEOUT "
/* PBC mode was activated */
   WPS_EVENT_ACTIVE,         // "WPS-PBC-ACTIVE "
/* PBC mode was disabled */
   WPS_EVENT_DISABLE,         // "WPS-PBC-DISABLE "

   WPS_EVENT_ENROLLEE_SEEN,         // "WPS-ENROLLEE-SEEN "

   WPS_EVENT_OPEN_NETWORK,         // "WPS-OPEN-NETWORK "
/** Result of SCS setup */
   WPA_EVENT_SCS_RESULT,         // "CTRL-EVENT-SCS-RESULT "
/* Event indicating DSCP policy */
   WPA_EVENT_DSCP_POLICY,         // "CTRL-EVENT-DSCP-POLICY "

/* WPS ER events */
   WPS_EVENT_ER_AP_ADD,         // "WPS-ER-AP-ADD "
   WPS_EVENT_ER_AP_REMOVE,         // "WPS-ER-AP-REMOVE "
   WPS_EVENT_ER_ENROLLEE_ADD,         // "WPS-ER-ENROLLEE-ADD "
   WPS_EVENT_ER_ENROLLEE_REMOVE,         // "WPS-ER-ENROLLEE-REMOVE "
   WPS_EVENT_ER_AP_SETTINGS,         // "WPS-ER-AP-SETTINGS "
   WPS_EVENT_ER_SET_SEL_REG,         // "WPS-ER-AP-SET-SEL-REG "

/* DPP events */
   DPP_EVENT_AUTH_SUCCESS,         // "DPP-AUTH-SUCCESS "
   DPP_EVENT_AUTH_INIT_FAILED,         // "DPP-AUTH-INIT-FAILED "
   DPP_EVENT_NOT_COMPATIBLE,         // "DPP-NOT-COMPATIBLE "
   DPP_EVENT_RESPONSE_PENDING,         // "DPP-RESPONSE-PENDING "
   DPP_EVENT_SCAN_PEER_QR_CODE,         // "DPP-SCAN-PEER-QR-CODE "
   DPP_EVENT_AUTH_DIRECTION,         // "DPP-AUTH-DIRECTION "
   DPP_EVENT_CONF_RECEIVED,         // "DPP-CONF-RECEIVED "
   DPP_EVENT_CONF_SENT,         // "DPP-CONF-SENT "
   DPP_EVENT_CONF_FAILED,         // "DPP-CONF-FAILED "
   DPP_EVENT_CONN_STATUS_RESULT,         // "DPP-CONN-STATUS-RESULT "
   DPP_EVENT_CONFOBJ_AKM,         // "DPP-CONFOBJ-AKM "
   DPP_EVENT_CONFOBJ_SSID,         // "DPP-CONFOBJ-SSID "
   DPP_EVENT_CONFOBJ_SSID_CHARSET,         // "DPP-CONFOBJ-SSID-CHARSET "
   DPP_EVENT_CONFOBJ_PASS,         // "DPP-CONFOBJ-PASS "
   DPP_EVENT_CONFOBJ_PSK,         // "DPP-CONFOBJ-PSK "
   DPP_EVENT_CONNECTOR,         // "DPP-CONNECTOR "
   DPP_EVENT_C_SIGN_KEY,         // "DPP-C-SIGN-KEY "
   DPP_EVENT_PP_KEY,         // "DPP-PP-KEY "
   DPP_EVENT_NET_ACCESS_KEY,         // "DPP-NET-ACCESS-KEY "
   DPP_EVENT_SERVER_NAME,         // "DPP-SERVER-NAME "
   DPP_EVENT_CERTBAG,         // "DPP-CERTBAG "
   DPP_EVENT_CACERT,         // "DPP-CACERT "
   DPP_EVENT_MISSING_CONNECTOR,         // "DPP-MISSING-CONNECTOR "
   DPP_EVENT_NETWORK_ID,         // "DPP-NETWORK-ID "
   DPP_EVENT_CONFIGURATOR_ID,         // "DPP-CONFIGURATOR-ID "
   DPP_EVENT_RX,         // "DPP-RX "
   DPP_EVENT_TX,         // "DPP-TX "
   DPP_EVENT_TX_STATUS,         // "DPP-TX-STATUS "
   DPP_EVENT_FAIL,         // "DPP-FAIL "
   DPP_EVENT_PKEX_T_LIMIT,         // "DPP-PKEX-T-LIMIT "
   DPP_EVENT_INTRO,         // "DPP-INTRO "
   DPP_EVENT_CONF_REQ_RX,         // "DPP-CONF-REQ-RX "
   DPP_EVENT_CHIRP_STOPPED,         // "DPP-CHIRP-STOPPED "
   DPP_EVENT_MUD_URL,         // "DPP-MUD-URL "
   DPP_EVENT_BAND_SUPPORT,         // "DPP-BAND-SUPPORT "
   DPP_EVENT_CSR,         // "DPP-CSR "
   DPP_EVENT_CHIRP_RX,         // "DPP-CHIRP-RX "

/* MESH events */
   MESH_GROUP_STARTED,         // "MESH-GROUP-STARTED "
   MESH_GROUP_REMOVED,         // "MESH-GROUP-REMOVED "
   MESH_PEER_CONNECTED,         // "MESH-PEER-CONNECTED "
   MESH_PEER_DISCONNECTED,         // "MESH-PEER-DISCONNECTED "
/** Mesh SAE authentication failure. Wrong password suspected. */
   MESH_SAE_AUTH_FAILURE,         // "MESH-SAE-AUTH-FAILURE "
   MESH_SAE_AUTH_BLOCKED,         // "MESH-SAE-AUTH-BLOCKED "

/* WMM AC events */
   WMM_AC_EVENT_TSPEC_ADDED,         // "TSPEC-ADDED "
   WMM_AC_EVENT_TSPEC_REMOVED,         // "TSPEC-REMOVED "
   WMM_AC_EVENT_TSPEC_REQ_FAILED,         // "TSPEC-REQ-FAILED "

/** P2P device found */
    P2P_EVENT_DEVICE_FOUND,         // "P2P-DEVICE-FOUND "

/** P2P device lost */
    P2P_EVENT_DEVICE_LOST,         // "P2P-DEVICE-LOST "

/** A P2P device requested GO negotiation, but we were not ready to start the
 * negotiation */
    P2P_EVENT_GO_NEG_REQUEST,         // "P2P-GO-NEG-REQUEST "
    P2P_EVENT_GO_NEG_SUCCESS,         // "P2P-GO-NEG-SUCCESS "
    P2P_EVENT_GO_NEG_FAILURE,         // "P2P-GO-NEG-FAILURE "
    P2P_EVENT_GROUP_FORMATION_SUCCESS,         // "P2P-GROUP-FORMATION-SUCCESS "
    P2P_EVENT_GROUP_FORMATION_FAILURE,         // "P2P-GROUP-FORMATION-FAILURE "
    P2P_EVENT_GROUP_STARTED,         // "P2P-GROUP-STARTED "
    P2P_EVENT_GROUP_REMOVED,         // "P2P-GROUP-REMOVED "
    P2P_EVENT_CROSS_CONNECT_ENABLE,         // "P2P-CROSS-CONNECT-ENABLE "
    P2P_EVENT_CROSS_CONNECT_DISABLE,         // "P2P-CROSS-CONNECT-DISABLE "
/* parameters: <peer address> <PIN> */
    P2P_EVENT_PROV_DISC_SHOW_PIN,         // "P2P-PROV-DISC-SHOW-PIN "
/* parameters: <peer address> */
    P2P_EVENT_PROV_DISC_ENTER_PIN,         // "P2P-PROV-DISC-ENTER-PIN "
/* parameters: <peer address> */
    P2P_EVENT_PROV_DISC_PBC_REQ,         // "P2P-PROV-DISC-PBC-REQ "
/* parameters: <peer address> */
    P2P_EVENT_PROV_DISC_PBC_RESP,         // "P2P-PROV-DISC-PBC-RESP "
/* parameters: <peer address> <status> */
    P2P_EVENT_PROV_DISC_FAILURE,         // "P2P-PROV-DISC-FAILURE"
/* parameters: <freq> <src addr> <dialog token> <update indicator> <TLVs> */
    P2P_EVENT_SERV_DISC_REQ,         // "P2P-SERV-DISC-REQ "
/* parameters: <src addr> <update indicator> <TLVs> */
    P2P_EVENT_SERV_DISC_RESP,         // "P2P-SERV-DISC-RESP "
    P2P_EVENT_SERV_ASP_RESP,         // "P2P-SERV-ASP-RESP "
    P2P_EVENT_INVITATION_RECEIVED,         // "P2P-INVITATION-RECEIVED "
    P2P_EVENT_INVITATION_RESULT,         // "P2P-INVITATION-RESULT "
    P2P_EVENT_INVITATION_ACCEPTED,         // "P2P-INVITATION-ACCEPTED "
    P2P_EVENT_FIND_STOPPED,         // "P2P-FIND-STOPPED "
    P2P_EVENT_PERSISTENT_PSK_FAIL,         // "P2P-PERSISTENT-PSK-FAIL id="
    P2P_EVENT_PRESENCE_RESPONSE,         // "P2P-PRESENCE-RESPONSE "
    P2P_EVENT_NFC_BOTH_GO,         // "P2P-NFC-BOTH-GO "
    P2P_EVENT_NFC_PEER_CLIENT,         // "P2P-NFC-PEER-CLIENT "
    P2P_EVENT_NFC_WHILE_CLIENT,         // "P2P-NFC-WHILE-CLIENT "
    P2P_EVENT_FALLBACK_TO_GO_NEG,         // "P2P-FALLBACK-TO-GO-NEG "
    P2P_EVENT_FALLBACK_TO_GO_NEG_ENABLED,         // "P2P-FALLBACK-TO-GO-NEG-ENABLED "

/* parameters: <PMF enabled> <timeout in ms> <Session Information URL> */
   ESS_DISASSOC_IMMINENT,         // "ESS-DISASSOC-IMMINENT "
    P2P_EVENT_REMOVE_AND_REFORM_GROUP,         // "P2P-REMOVE-AND-REFORM-GROUP "

    P2P_EVENT_P2PS_PROVISION_START,         // "P2PS-PROV-START "
    P2P_EVENT_P2PS_PROVISION_DONE,         // "P2PS-PROV-DONE "

   INTERWORKING_AP,         // "INTERWORKING-AP "
   INTERWORKING_EXCLUDED,         // "INTERWORKING-BLACKLISTED "
   INTERWORKING_NO_MATCH,         // "INTERWORKING-NO-MATCH "
   INTERWORKING_ALREADY_CONNECTED,         // "INTERWORKING-ALREADY-CONNECTED "
   INTERWORKING_SELECTED,         // "INTERWORKING-SELECTED "

/* Credential block added; parameters: <id> */
   CRED_ADDED,         // "CRED-ADDED "
/* Credential block modified; parameters: <id> <field> */
   CRED_MODIFIED,         // "CRED-MODIFIED "
/* Credential block removed; parameters: <id> */
   CRED_REMOVED,         // "CRED-REMOVED "

   GAS_RESPONSE_INFO,         // "GAS-RESPONSE-INFO "
/* parameters: <addr> <dialog_token> <freq> */
   GAS_QUERY_START,         // "GAS-QUERY-START "
/* parameters: <addr> <dialog_token> <freq> <status_code> <result> */
   GAS_QUERY_DONE,         // "GAS-QUERY-DONE "

/* parameters: <addr> <result> */
   ANQP_QUERY_DONE,         // "ANQP-QUERY-DONE "

   RX_ANQP,         // "RX-ANQP "
    RX_HS20_ANQP,         // "RX-HS20-ANQP "
    RX_HS20_ANQP_ICON,         // "RX-HS20-ANQP-ICON "
    RX_HS20_ICON,         // "RX-HS20-ICON "
   RX_MBO_ANQP,         // "RX-MBO-ANQP "

/* parameters: <Venue Number> <Venue URL> */
   RX_VENUE_URL,         // "RX-VENUE-URL "

    HS20_SUBSCRIPTION_REMEDIATION,         // "HS20-SUBSCRIPTION-REMEDIATION "
    HS20_DEAUTH_IMMINENT_NOTICE,         // "HS20-DEAUTH-IMMINENT-NOTICE "
    HS20_T_C_ACCEPTANCE,         // "HS20-T-C-ACCEPTANCE "

   EXT_RADIO_WORK_START,         // "EXT-RADIO-WORK-START "
   EXT_RADIO_WORK_TIMEOUT,         // "EXT-RADIO-WORK-TIMEOUT "

   RRM_EVENT_NEIGHBOR_REP_RXED,         // "RRM-NEIGHBOR-REP-RECEIVED "
   RRM_EVENT_NEIGHBOR_REP_FAILED,         // "RRM-NEIGHBOR-REP-REQUEST-FAILED "

/* hostapd control interface - fixed message prefixes */
   WPS_EVENT_PIN_NEEDED,         // "WPS-PIN-NEEDED "
   WPS_EVENT_NEW_AP_SETTINGS,         // "WPS-NEW-AP-SETTINGS "
   WPS_EVENT_REG_SUCCESS,         // "WPS-REG-SUCCESS "
   WPS_EVENT_AP_SETUP_LOCKED,         // "WPS-AP-SETUP-LOCKED "
   WPS_EVENT_AP_SETUP_UNLOCKED,         // "WPS-AP-SETUP-UNLOCKED "
   WPS_EVENT_AP_PIN_ENABLED,         // "WPS-AP-PIN-ENABLED "
   WPS_EVENT_AP_PIN_DISABLED,         // "WPS-AP-PIN-DISABLED "
   WPS_EVENT_PIN_ACTIVE,         // "WPS-PIN-ACTIVE "
   WPS_EVENT_CANCEL,         // "WPS-CANCEL "
   AP_STA_CONNECTED,         // "AP-STA-CONNECTED "
   AP_STA_DISCONNECTED,         // "AP-STA-DISCONNECTED "
   AP_STA_POSSIBLE_PSK_MISMATCH,         // "AP-STA-POSSIBLE-PSK-MISMATCH "
   AP_STA_POLL_OK,         // "AP-STA-POLL-OK "

   AP_REJECTED_MAX_STA,         // "AP-REJECTED-MAX-STA "
   AP_REJECTED_BLOCKED_STA,         // "AP-REJECTED-BLOCKED-STA "

    HS20_T_C_FILTERING_ADD,         // "HS20-T-C-FILTERING-ADD "
    HS20_T_C_FILTERING_REMOVE,         // "HS20-T-C-FILTERING-REMOVE "

   AP_EVENT_ENABLED,         // "AP-ENABLED "
   AP_EVENT_DISABLED,         // "AP-DISABLED "

   INTERFACE_ENABLED,         // "INTERFACE-ENABLED "
   INTERFACE_DISABLED,         // "INTERFACE-DISABLED "

   ACS_EVENT_STARTED,         // "ACS-STARTED "
   ACS_EVENT_COMPLETED,         // "ACS-COMPLETED "
   ACS_EVENT_FAILED,         // "ACS-FAILED "

   DFS_EVENT_RADAR_DETECTED,         // "DFS-RADAR-DETECTED "
   DFS_EVENT_NEW_CHANNEL,         // "DFS-NEW-CHANNEL "
   DFS_EVENT_CAC_START,         // "DFS-CAC-START "
   DFS_EVENT_CAC_COMPLETED,         // "DFS-CAC-COMPLETED "
   DFS_EVENT_NOP_FINISHED,         // "DFS-NOP-FINISHED "
   DFS_EVENT_PRE_CAC_EXPIRED,         // "DFS-PRE-CAC-EXPIRED "

   AP_CSA_FINISHED,         // "AP-CSA-FINISHED "

    P2P_EVENT_LISTEN_OFFLOAD_STOP,         // "P2P-LISTEN-OFFLOAD-STOPPED "
    P2P_LISTEN_OFFLOAD_STOP_REASON,         // "P2P-LISTEN-OFFLOAD-STOP-REASON "

/* BSS Transition Management Response frame received */
   BSS_TM_RESP,         // "BSS-TM-RESP "

/* Collocated Interference Request frame received;
 * parameters: <dialog token> <automatic report enabled> <report timeout> */
   COLOC_INTF_REQ,         // "COLOC-INTF-REQ "
/* Collocated Interference Report frame received;
 * parameters: <STA address> <dialog token> <hexdump of report elements> */
   COLOC_INTF_REPORT,         // "COLOC-INTF-REPORT "

/* MBO IE with cellular data connection preference received */
   MBO_CELL_PREFERENCE,         // "MBO-CELL-PREFERENCE "

/* BSS Transition Management Request received with MBO transition reason */
   MBO_TRANSITION_REASON,         // "MBO-TRANSITION-REASON "

/* parameters: <STA address> <dialog token> <ack=0/1> */
   BEACON_REQ_TX_STATUS,         // "BEACON-REQ-TX-STATUS "
/* parameters: <STA address> <dialog token> <report mode> <beacon report> */
   BEACON_RESP_RX,         // "BEACON-RESP-RX "

/* PMKSA cache entry added; parameters: <BSSID> <network_id> */
   PMKSA_CACHE_ADDED,         // "PMKSA-CACHE-ADDED "
/* PMKSA cache entry removed; parameters: <BSSID> <network_id> */
   PMKSA_CACHE_REMOVED,         // "PMKSA-CACHE-REMOVED "

/* FILS HLP Container receive; parameters: dst=<addr> src=<addr> frame=<hexdump>
 */
   FILS_HLP_RX,         // "FILS-HLP-RX "

/* Event to indicate Probe Request frame;
 * parameters: sa=<STA MAC address> signal=<signal> */
   RX_PROBE_REQUEST,         // "RX-PROBE-REQUEST "

/* Event to indicate station's HT/VHT operation mode change information */
   STA_OPMODE_MAX_BW_CHANGED,         // "STA-OPMODE-MAX-BW-CHANGED "
   STA_OPMODE_SMPS_MODE_CHANGED,         // "STA-OPMODE-SMPS-MODE-CHANGED "
   STA_OPMODE_N_SS_CHANGED,         // "STA-OPMODE-N_SS-CHANGED "

/* New interface addition or removal for 4addr WDS SDA */
   WDS_STA_INTERFACE_ADDED,         // "WDS-STA-INTERFACE-ADDED "
   WDS_STA_INTERFACE_REMOVED,         // "WDS-STA-INTERFACE-REMOVED "

/* Transition mode disabled indication - followed by bitmap */
   TRANSITION_DISABLE,         // "TRANSITION-DISABLE "

/* OCV validation failure; parameters: addr=<src addr>
 * frame=<saqueryreq/saqueryresp> error=<error string> */
   OCV_FAILURE,         // "OCV-FAILURE "

/* Event triggered for received management frame */
   AP_MGMT_FRAME_RECEIVED,         // "AP-MGMT-FRAME-RECEIVED "
};

WpaEventMessage GetWpaEventMessage(const std::string &message);


} // namespace