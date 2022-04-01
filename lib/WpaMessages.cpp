#include"cotask/WpaMessages.h"
#include <unordered_map>

using namespace cotask;

static std::unordered_map<std::string, WpaEventMessage> stringToWpaEventMessage {{
    {"UNKNOWN", WpaEventMessage::WPA_UNKOWN_MESSAGE},
        /** Interactive request for identity/password/pin */
        {"CTRL-REQ-", WpaEventMessage::WPA_CTRL_REQ},

        /** Response to identity/password/pin request */
        {"CTRL-RSP-", WpaEventMessage::WPA_CTRL_RSP},

        /* Event messages with fixed prefix */
        /** Authentication completed successfully and data connection enabled */
        {"CTRL-EVENT-CONNECTED", WpaEventMessage::WPA_EVENT_CONNECTED},
        /** Disconnected, data connection is not available */
        {"CTRL-EVENT-DISCONNECTED", WpaEventMessage::WPA_EVENT_DISCONNECTED},
        /** Association rejected during connection attempt */
        {"CTRL-EVENT-ASSOC-REJECT", WpaEventMessage::WPA_EVENT_ASSOC_REJECT},
        /** Authentication rejected during connection attempt */
        {"CTRL-EVENT-AUTH-REJECT", WpaEventMessage::WPA_EVENT_AUTH_REJECT},
        /** wpa_supplicant is exiting */
        {"CTRL-EVENT-TERMINATING", WpaEventMessage::WPA_EVENT_TERMINATING},
        /** Password change was completed successfully */
        {"CTRL-EVENT-PASSWORD-CHANGED", WpaEventMessage::WPA_EVENT_PASSWORD_CHANGED},
        /** EAP-Request/Notification received */
        {"CTRL-EVENT-EAP-NOTIFICATION", WpaEventMessage::WPA_EVENT_EAP_NOTIFICATION},
        /** EAP authentication started (EAP-Request/Identity received) */
        {"CTRL-EVENT-EAP-STARTED", WpaEventMessage::WPA_EVENT_EAP_STARTED},
        /** EAP method proposed by the server */
        {"CTRL-EVENT-EAP-PROPOSED-METHOD", WpaEventMessage::WPA_EVENT_EAP_PROPOSED_METHOD},
        /** EAP method selected */
        {"CTRL-EVENT-EAP-METHOD", WpaEventMessage::WPA_EVENT_EAP_METHOD},
        /** EAP peer certificate from TLS */
        {"CTRL-EVENT-EAP-PEER-CERT", WpaEventMessage::WPA_EVENT_EAP_PEER_CERT},
        /** EAP peer certificate alternative subject name component from TLS */
        {"CTRL-EVENT-EAP-PEER-ALT", WpaEventMessage::WPA_EVENT_EAP_PEER_ALT},
        /** EAP TLS certificate chain validation error */
        {"CTRL-EVENT-EAP-TLS-CERT-ERROR", WpaEventMessage::WPA_EVENT_EAP_TLS_CERT_ERROR},
        /** EAP status */
        {"CTRL-EVENT-EAP-STATUS", WpaEventMessage::WPA_EVENT_EAP_STATUS},
        /** Retransmit the previous request packet */
        {"CTRL-EVENT-EAP-RETRANSMIT", WpaEventMessage::WPA_EVENT_EAP_RETRANSMIT},
        {"CTRL-EVENT-EAP-RETRANSMIT2", WpaEventMessage::WPA_EVENT_EAP_RETRANSMIT2},
        /** EAP authentication completed successfully */
        {"CTRL-EVENT-EAP-SUCCESS", WpaEventMessage::WPA_EVENT_EAP_SUCCESS},
        {"CTRL-EVENT-EAP-SUCCESS2", WpaEventMessage::WPA_EVENT_EAP_SUCCESS2},
        /** EAP authentication failed (EAP-Failure received) */
        {"CTRL-EVENT-EAP-FAILURE", WpaEventMessage::WPA_EVENT_EAP_FAILURE},
        {"CTRL-EVENT-EAP-FAILURE2", WpaEventMessage::WPA_EVENT_EAP_FAILURE2},
        /** EAP authentication failed due to no response received */
        {"CTRL-EVENT-EAP-TIMEOUT-FAILURE", WpaEventMessage::WPA_EVENT_EAP_TIMEOUT_FAILURE},
        {"CTRL-EVENT-EAP-TIMEOUT-FAILURE2", WpaEventMessage::WPA_EVENT_EAP_TIMEOUT_FAILURE2},
        {"EAP-ERROR-CODE", WpaEventMessage::WPA_EVENT_EAP_ERROR_CODE},
        /** Network block temporarily disabled (e.g., due to authentication failure) */
        {"CTRL-EVENT-SSID-TEMP-DISABLED", WpaEventMessage::WPA_EVENT_TEMP_DISABLED},
        /** Temporarily disabled network block re-enabled */
        {"CTRL-EVENT-SSID-REENABLED", WpaEventMessage::WPA_EVENT_REENABLED},
        /** New scan started */
        {"CTRL-EVENT-SCAN-STARTED", WpaEventMessage::WPA_EVENT_SCAN_STARTED},
        /** New scan results available */
        {"CTRL-EVENT-SCAN-RESULTS", WpaEventMessage::WPA_EVENT_SCAN_RESULTS},
        /** Scan command failed */
        {"CTRL-EVENT-SCAN-FAILED", WpaEventMessage::WPA_EVENT_SCAN_FAILED},
        /** wpa_supplicant state change */
        {"CTRL-EVENT-STATE-CHANGE", WpaEventMessage::WPA_EVENT_STATE_CHANGE},
        /** A new BSS entry was added (followed by BSS entry id and BSSID) */
        {"CTRL-EVENT-BSS-ADDED", WpaEventMessage::WPA_EVENT_BSS_ADDED},
        /** A BSS entry was removed (followed by BSS entry id and BSSID) */
        {"CTRL-EVENT-BSS-REMOVED", WpaEventMessage::WPA_EVENT_BSS_REMOVED},
        /** No suitable network was found */
        {"CTRL-EVENT-NETWORK-NOT-FOUND", WpaEventMessage::WPA_EVENT_NETWORK_NOT_FOUND},
        /** Change in the signal level was reported by the driver */
        {"CTRL-EVENT-SIGNAL-CHANGE", WpaEventMessage::WPA_EVENT_SIGNAL_CHANGE},
        /** Beacon loss reported by the driver */
        {"CTRL-EVENT-BEACON-LOSS", WpaEventMessage::WPA_EVENT_BEACON_LOSS},
        /** Regulatory domain channel */
        {"CTRL-EVENT-REGDOM-CHANGE", WpaEventMessage::WPA_EVENT_REGDOM_CHANGE},
        /** Channel switch started (followed by freq=<MHz> and other channel parameters)
         */
        {"CTRL-EVENT-STARTED-CHANNEL-SWITCH", WpaEventMessage::WPA_EVENT_CHANNEL_SWITCH_STARTED},
        /** Channel switch (followed by freq=<MHz> and other channel parameters) */
        {"CTRL-EVENT-CHANNEL-SWITCH", WpaEventMessage::WPA_EVENT_CHANNEL_SWITCH},
        /** SAE authentication failed due to unknown password identifier */
        {"CTRL-EVENT-SAE-UNKNOWN-PASSWORD-IDENTIFIER", WpaEventMessage::WPA_EVENT_SAE_UNKNOWN_PASSWORD_IDENTIFIER},

        /** Unprotected Beacon frame dropped */
        {"CTRL-EVENT-UNPROT-BEACON", WpaEventMessage::WPA_EVENT_UNPROT_BEACON},
        /** Decision made to do a within-ESS roam */
        {"CTRL-EVENT-DO-ROAM", WpaEventMessage::WPA_EVENT_DO_ROAM},
        /** Decision made to skip a within-ESS roam */
        {"CTRL-EVENT-SKIP-ROAM", WpaEventMessage::WPA_EVENT_SKIP_ROAM},

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
        {"CTRL-EVENT-SUBNET-STATUS-UPDATE", WpaEventMessage::WPA_EVENT_SUBNET_STATUS_UPDATE},

        /** RSN IBSS 4-way handshakes completed with specified peer */
        {"IBSS-RSN-COMPLETED", WpaEventMessage::IBSS_RSN_COMPLETED},

        /** Notification of frequency conflict due to a concurrent operation.
         *
         * The indicated network is disabled and needs to be re-enabled before it can
         * be used again.
         */
        {"CTRL-EVENT-FREQ-CONFLICT", WpaEventMessage::WPA_EVENT_FREQ_CONFLICT},
        /** Frequency ranges that the driver recommends to avoid */
        {"CTRL-EVENT-AVOID-FREQ", WpaEventMessage::WPA_EVENT_AVOID_FREQ},
        /** A new network profile was added (followed by network entry id) */
        {"CTRL-EVENT-NETWORK-ADDED", WpaEventMessage::WPA_EVENT_NETWORK_ADDED},
        /** A network profile was removed (followed by prior network entry id) */
        {"CTRL-EVENT-NETWORK-REMOVED", WpaEventMessage::WPA_EVENT_NETWORK_REMOVED},
        /** Result of MSCS setup */
        {"CTRL-EVENT-MSCS-RESULT", WpaEventMessage::WPA_EVENT_MSCS_RESULT},
        /** WPS overlap detected in PBC mode */
        {"WPS-OVERLAP-DETECTED", WpaEventMessage::WPS_EVENT_OVERLAP},
        /** Available WPS AP with active PBC found in scan results */
        {"WPS-AP-AVAILABLE-PBC", WpaEventMessage::WPS_EVENT_AP_AVAILABLE_PBC},
        /** Available WPS AP with our address as authorized in scan results */
        {"WPS-AP-AVAILABLE-AUTH", WpaEventMessage::WPS_EVENT_AP_AVAILABLE_AUTH},
        /** Available WPS AP with recently selected PIN registrar found in scan results
         */
        {"WPS-AP-AVAILABLE-PIN", WpaEventMessage::WPS_EVENT_AP_AVAILABLE_PIN},
        /** Available WPS AP found in scan results */
        {"WPS-AP-AVAILABLE", WpaEventMessage::WPS_EVENT_AP_AVAILABLE},
        /** A new credential received */
        {"WPS-CRED-RECEIVED", WpaEventMessage::WPS_EVENT_CRED_RECEIVED},
        /** M2D received */
        {"WPS-M2D", WpaEventMessage::WPS_EVENT_M2D},

        /** WPS registration failed after M2/M2D */
        {"WPS-FAIL", WpaEventMessage::WPS_EVENT_FAIL},
        /** WPS registration completed successfully */
        {"WPS-SUCCESS", WpaEventMessage::WPS_EVENT_SUCCESS},
        /** WPS enrollment attempt timed out and was terminated */
        {"WPS-TIMEOUT", WpaEventMessage::WPS_EVENT_TIMEOUT},
        /* PBC mode was activated */
        {"WPS-PBC-ACTIVE", WpaEventMessage::WPS_EVENT_ACTIVE},
        /* PBC mode was disabled */
        {"WPS-PBC-DISABLE", WpaEventMessage::WPS_EVENT_DISABLE},

        {"WPS-ENROLLEE-SEEN", WpaEventMessage::WPS_EVENT_ENROLLEE_SEEN},

        {"WPS-OPEN-NETWORK", WpaEventMessage::WPS_EVENT_OPEN_NETWORK},
        /** Result of SCS setup */
        {"CTRL-EVENT-SCS-RESULT", WpaEventMessage::WPA_EVENT_SCS_RESULT},
        /* Event indicating DSCP policy */
        {"CTRL-EVENT-DSCP-POLICY", WpaEventMessage::WPA_EVENT_DSCP_POLICY},

        /* WPS ER events */
        {"WPS-ER-AP-ADD", WpaEventMessage::WPS_EVENT_ER_AP_ADD},
        {"WPS-ER-AP-REMOVE", WpaEventMessage::WPS_EVENT_ER_AP_REMOVE},
        {"WPS-ER-ENROLLEE-ADD", WpaEventMessage::WPS_EVENT_ER_ENROLLEE_ADD},
        {"WPS-ER-ENROLLEE-REMOVE", WpaEventMessage::WPS_EVENT_ER_ENROLLEE_REMOVE},
        {"WPS-ER-AP-SETTINGS", WpaEventMessage::WPS_EVENT_ER_AP_SETTINGS},
        {"WPS-ER-AP-SET-SEL-REG", WpaEventMessage::WPS_EVENT_ER_SET_SEL_REG},

        /* DPP events */
        {"DPP-AUTH-SUCCESS", WpaEventMessage::DPP_EVENT_AUTH_SUCCESS},
        {"DPP-AUTH-INIT-FAILED", WpaEventMessage::DPP_EVENT_AUTH_INIT_FAILED},
        {"DPP-NOT-COMPATIBLE", WpaEventMessage::DPP_EVENT_NOT_COMPATIBLE},
        {"DPP-RESPONSE-PENDING", WpaEventMessage::DPP_EVENT_RESPONSE_PENDING},
        {"DPP-SCAN-PEER-QR-CODE", WpaEventMessage::DPP_EVENT_SCAN_PEER_QR_CODE},
        {"DPP-AUTH-DIRECTION", WpaEventMessage::DPP_EVENT_AUTH_DIRECTION},
        {"DPP-CONF-RECEIVED", WpaEventMessage::DPP_EVENT_CONF_RECEIVED},
        {"DPP-CONF-SENT", WpaEventMessage::DPP_EVENT_CONF_SENT},
        {"DPP-CONF-FAILED", WpaEventMessage::DPP_EVENT_CONF_FAILED},
        {"DPP-CONN-STATUS-RESULT", WpaEventMessage::DPP_EVENT_CONN_STATUS_RESULT},
        {"DPP-CONFOBJ-AKM", WpaEventMessage::DPP_EVENT_CONFOBJ_AKM},
        {"DPP-CONFOBJ-SSID", WpaEventMessage::DPP_EVENT_CONFOBJ_SSID},
        {"DPP-CONFOBJ-SSID-CHARSET", WpaEventMessage::DPP_EVENT_CONFOBJ_SSID_CHARSET},
        {"DPP-CONFOBJ-PASS", WpaEventMessage::DPP_EVENT_CONFOBJ_PASS},
        {"DPP-CONFOBJ-PSK", WpaEventMessage::DPP_EVENT_CONFOBJ_PSK},
        {"DPP-CONNECTOR", WpaEventMessage::DPP_EVENT_CONNECTOR},
        {"DPP-C-SIGN-KEY", WpaEventMessage::DPP_EVENT_C_SIGN_KEY},
        {"DPP-PP-KEY", WpaEventMessage::DPP_EVENT_PP_KEY},
        {"DPP-NET-ACCESS-KEY", WpaEventMessage::DPP_EVENT_NET_ACCESS_KEY},
        {"DPP-SERVER-NAME", WpaEventMessage::DPP_EVENT_SERVER_NAME},
        {"DPP-CERTBAG", WpaEventMessage::DPP_EVENT_CERTBAG},
        {"DPP-CACERT", WpaEventMessage::DPP_EVENT_CACERT},
        {"DPP-MISSING-CONNECTOR", WpaEventMessage::DPP_EVENT_MISSING_CONNECTOR},
        {"DPP-NETWORK-ID", WpaEventMessage::DPP_EVENT_NETWORK_ID},
        {"DPP-CONFIGURATOR-ID", WpaEventMessage::DPP_EVENT_CONFIGURATOR_ID},
        {"DPP-RX", WpaEventMessage::DPP_EVENT_RX},
        {"DPP-TX", WpaEventMessage::DPP_EVENT_TX},
        {"DPP-TX-STATUS", WpaEventMessage::DPP_EVENT_TX_STATUS},
        {"DPP-FAIL", WpaEventMessage::DPP_EVENT_FAIL},
        {"DPP-PKEX-T-LIMIT", WpaEventMessage::DPP_EVENT_PKEX_T_LIMIT},
        {"DPP-INTRO", WpaEventMessage::DPP_EVENT_INTRO},
        {"DPP-CONF-REQ-RX", WpaEventMessage::DPP_EVENT_CONF_REQ_RX},
        {"DPP-CHIRP-STOPPED", WpaEventMessage::DPP_EVENT_CHIRP_STOPPED},
        {"DPP-MUD-URL", WpaEventMessage::DPP_EVENT_MUD_URL},
        {"DPP-BAND-SUPPORT", WpaEventMessage::DPP_EVENT_BAND_SUPPORT},
        {"DPP-CSR", WpaEventMessage::DPP_EVENT_CSR},
        {"DPP-CHIRP-RX", WpaEventMessage::DPP_EVENT_CHIRP_RX},

        /* MESH events */
        {"MESH-GROUP-STARTED", WpaEventMessage::MESH_GROUP_STARTED},
        {"MESH-GROUP-REMOVED", WpaEventMessage::MESH_GROUP_REMOVED},
        {"MESH-PEER-CONNECTED", WpaEventMessage::MESH_PEER_CONNECTED},
        {"MESH-PEER-DISCONNECTED", WpaEventMessage::MESH_PEER_DISCONNECTED},
        /** Mesh SAE authentication failure. Wrong password suspected. */
        {"MESH-SAE-AUTH-FAILURE", WpaEventMessage::MESH_SAE_AUTH_FAILURE},
        {"MESH-SAE-AUTH-BLOCKED", WpaEventMessage::MESH_SAE_AUTH_BLOCKED},

        /* WMM AC events */
        {"TSPEC-ADDED", WpaEventMessage::WMM_AC_EVENT_TSPEC_ADDED},
        {"TSPEC-REMOVED", WpaEventMessage::WMM_AC_EVENT_TSPEC_REMOVED},
        {"TSPEC-REQ-FAILED", WpaEventMessage::WMM_AC_EVENT_TSPEC_REQ_FAILED},

        /** P2P device found */
        {"P2P-DEVICE-FOUND", WpaEventMessage::P2P_EVENT_DEVICE_FOUND},

        /** P2P device lost */
        {"P2P-DEVICE-LOST", WpaEventMessage::P2P_EVENT_DEVICE_LOST},

        /** A P2P device requested GO negotiation, but we were not ready to start the
         * negotiation */
        {"P2P-GO-NEG-REQUEST", WpaEventMessage::P2P_EVENT_GO_NEG_REQUEST},
        {"P2P-GO-NEG-SUCCESS", WpaEventMessage::P2P_EVENT_GO_NEG_SUCCESS},
        {"P2P-GO-NEG-FAILURE", WpaEventMessage::P2P_EVENT_GO_NEG_FAILURE},
        {"P2P-GROUP-FORMATION-SUCCESS", WpaEventMessage::P2P_EVENT_GROUP_FORMATION_SUCCESS},
        {"P2P-GROUP-FORMATION-FAILURE", WpaEventMessage::P2P_EVENT_GROUP_FORMATION_FAILURE},
        {"P2P-GROUP-STARTED", WpaEventMessage::P2P_EVENT_GROUP_STARTED},
        {"P2P-GROUP-REMOVED", WpaEventMessage::P2P_EVENT_GROUP_REMOVED},
        {"P2P-CROSS-CONNECT-ENABLE", WpaEventMessage::P2P_EVENT_CROSS_CONNECT_ENABLE},
        {"P2P-CROSS-CONNECT-DISABLE", WpaEventMessage::P2P_EVENT_CROSS_CONNECT_DISABLE},
        /* parameters: <peer address> <PIN> */
        {"P2P-PROV-DISC-SHOW-PIN", WpaEventMessage::P2P_EVENT_PROV_DISC_SHOW_PIN},
        /* parameters: <peer address> */
        {"P2P-PROV-DISC-ENTER-PIN", WpaEventMessage::P2P_EVENT_PROV_DISC_ENTER_PIN},
        /* parameters: <peer address> */
        {"P2P-PROV-DISC-PBC-REQ", WpaEventMessage::P2P_EVENT_PROV_DISC_PBC_REQ},
        /* parameters: <peer address> */
        {"P2P-PROV-DISC-PBC-RESP", WpaEventMessage::P2P_EVENT_PROV_DISC_PBC_RESP},
        /* parameters: <peer address> <status> */
        {"P2P-PROV-DISC-FAILURE", WpaEventMessage::P2P_EVENT_PROV_DISC_FAILURE},
        /* parameters: <freq> <src addr> <dialog token> <update indicator> <TLVs> */
        {"P2P-SERV-DISC-REQ", WpaEventMessage::P2P_EVENT_SERV_DISC_REQ},
        /* parameters: <src addr> <update indicator> <TLVs> */
        {"P2P-SERV-DISC-RESP", WpaEventMessage::P2P_EVENT_SERV_DISC_RESP},
        {"P2P-SERV-ASP-RESP", WpaEventMessage::P2P_EVENT_SERV_ASP_RESP},
        {"P2P-INVITATION-RECEIVED", WpaEventMessage::P2P_EVENT_INVITATION_RECEIVED},
        {"P2P-INVITATION-RESULT", WpaEventMessage::P2P_EVENT_INVITATION_RESULT},
        {"P2P-INVITATION-ACCEPTED", WpaEventMessage::P2P_EVENT_INVITATION_ACCEPTED},
        {"P2P-FIND-STOPPED", WpaEventMessage::P2P_EVENT_FIND_STOPPED},
        {"P2P-PERSISTENT-PSK-FAIL id=", WpaEventMessage::P2P_EVENT_PERSISTENT_PSK_FAIL},
        {"P2P-PRESENCE-RESPONSE", WpaEventMessage::P2P_EVENT_PRESENCE_RESPONSE},
        {"P2P-NFC-BOTH-GO", WpaEventMessage::P2P_EVENT_NFC_BOTH_GO},
        {"P2P-NFC-PEER-CLIENT", WpaEventMessage::P2P_EVENT_NFC_PEER_CLIENT},
        {"P2P-NFC-WHILE-CLIENT", WpaEventMessage::P2P_EVENT_NFC_WHILE_CLIENT},
        {"P2P-FALLBACK-TO-GO-NEG", WpaEventMessage::P2P_EVENT_FALLBACK_TO_GO_NEG},
        {"P2P-FALLBACK-TO-GO-NEG-ENABLED", WpaEventMessage::P2P_EVENT_FALLBACK_TO_GO_NEG_ENABLED},

        /* parameters: <PMF enabled> <timeout in ms> <Session Information URL> */
        {"ESS-DISASSOC-IMMINENT", WpaEventMessage::ESS_DISASSOC_IMMINENT},
        {"P2P-REMOVE-AND-REFORM-GROUP", WpaEventMessage::P2P_EVENT_REMOVE_AND_REFORM_GROUP},

        {"P2PS-PROV-START", WpaEventMessage::P2P_EVENT_P2PS_PROVISION_START},
        {"P2PS-PROV-DONE", WpaEventMessage::P2P_EVENT_P2PS_PROVISION_DONE},

        {"INTERWORKING-AP", WpaEventMessage::INTERWORKING_AP},
        {"INTERWORKING-BLACKLISTED", WpaEventMessage::INTERWORKING_EXCLUDED},
        {"INTERWORKING-NO-MATCH", WpaEventMessage::INTERWORKING_NO_MATCH},
        {"INTERWORKING-ALREADY-CONNECTED", WpaEventMessage::INTERWORKING_ALREADY_CONNECTED},
        {"INTERWORKING-SELECTED", WpaEventMessage::INTERWORKING_SELECTED},

        /* Credential block added; parameters: <id> */
        {"CRED-ADDED", WpaEventMessage::CRED_ADDED},
        /* Credential block modified; parameters: <id> <field> */
        {"CRED-MODIFIED", WpaEventMessage::CRED_MODIFIED},
        /* Credential block removed; parameters: <id> */
        {"CRED-REMOVED", WpaEventMessage::CRED_REMOVED},

        {"GAS-RESPONSE-INFO", WpaEventMessage::GAS_RESPONSE_INFO},
        /* parameters: <addr> <dialog_token> <freq> */
        {"GAS-QUERY-START", WpaEventMessage::GAS_QUERY_START},
        /* parameters: <addr> <dialog_token> <freq> <status_code> <result> */
        {"GAS-QUERY-DONE", WpaEventMessage::GAS_QUERY_DONE},

        /* parameters: <addr> <result> */
        {"ANQP-QUERY-DONE", WpaEventMessage::ANQP_QUERY_DONE},

        {"RX-ANQP", WpaEventMessage::RX_ANQP},
        {"RX-HS20-ANQP", WpaEventMessage::RX_HS20_ANQP},
        {"RX-HS20-ANQP-ICON", WpaEventMessage::RX_HS20_ANQP_ICON},
        {"RX-HS20-ICON", WpaEventMessage::RX_HS20_ICON},
        {"RX-MBO-ANQP", WpaEventMessage::RX_MBO_ANQP},

        /* parameters: <Venue Number> <Venue URL> */
        {"RX-VENUE-URL", WpaEventMessage::RX_VENUE_URL},

        {"HS20-SUBSCRIPTION-REMEDIATION", WpaEventMessage::HS20_SUBSCRIPTION_REMEDIATION},
        {"HS20-DEAUTH-IMMINENT-NOTICE", WpaEventMessage::HS20_DEAUTH_IMMINENT_NOTICE},
        {"HS20-T-C-ACCEPTANCE", WpaEventMessage::HS20_T_C_ACCEPTANCE},

        {"EXT-RADIO-WORK-START", WpaEventMessage::EXT_RADIO_WORK_START},
        {"EXT-RADIO-WORK-TIMEOUT", WpaEventMessage::EXT_RADIO_WORK_TIMEOUT},

        {"RRM-NEIGHBOR-REP-RECEIVED", WpaEventMessage::RRM_EVENT_NEIGHBOR_REP_RXED},
        {"RRM-NEIGHBOR-REP-REQUEST-FAILED", WpaEventMessage::RRM_EVENT_NEIGHBOR_REP_FAILED},

        /* hostapd control interface - fixed message prefixes */
        {"WPS-PIN-NEEDED", WpaEventMessage::WPS_EVENT_PIN_NEEDED},
        {"WPS-NEW-AP-SETTINGS", WpaEventMessage::WPS_EVENT_NEW_AP_SETTINGS},
        {"WPS-REG-SUCCESS", WpaEventMessage::WPS_EVENT_REG_SUCCESS},
        {"WPS-AP-SETUP-LOCKED", WpaEventMessage::WPS_EVENT_AP_SETUP_LOCKED},
        {"WPS-AP-SETUP-UNLOCKED", WpaEventMessage::WPS_EVENT_AP_SETUP_UNLOCKED},
        {"WPS-AP-PIN-ENABLED", WpaEventMessage::WPS_EVENT_AP_PIN_ENABLED},
        {"WPS-AP-PIN-DISABLED", WpaEventMessage::WPS_EVENT_AP_PIN_DISABLED},
        {"WPS-PIN-ACTIVE", WpaEventMessage::WPS_EVENT_PIN_ACTIVE},
        {"WPS-CANCEL", WpaEventMessage::WPS_EVENT_CANCEL},
        {"AP-STA-CONNECTED", WpaEventMessage::AP_STA_CONNECTED},
        {"AP-STA-DISCONNECTED", WpaEventMessage::AP_STA_DISCONNECTED},
        {"AP-STA-POSSIBLE-PSK-MISMATCH", WpaEventMessage::AP_STA_POSSIBLE_PSK_MISMATCH},
        {"AP-STA-POLL-OK", WpaEventMessage::AP_STA_POLL_OK},

        {"AP-REJECTED-MAX-STA", WpaEventMessage::AP_REJECTED_MAX_STA},
        {"AP-REJECTED-BLOCKED-STA", WpaEventMessage::AP_REJECTED_BLOCKED_STA},

        {"HS20-T-C-FILTERING-ADD", WpaEventMessage::HS20_T_C_FILTERING_ADD},
        {"HS20-T-C-FILTERING-REMOVE", WpaEventMessage::HS20_T_C_FILTERING_REMOVE},

        {"AP-ENABLED", WpaEventMessage::AP_EVENT_ENABLED},
        {"AP-DISABLED", WpaEventMessage::AP_EVENT_DISABLED},

        {"INTERFACE-ENABLED", WpaEventMessage::INTERFACE_ENABLED},
        {"INTERFACE-DISABLED", WpaEventMessage::INTERFACE_DISABLED},

        {"ACS-STARTED", WpaEventMessage::ACS_EVENT_STARTED},
        {"ACS-COMPLETED", WpaEventMessage::ACS_EVENT_COMPLETED},
        {"ACS-FAILED", WpaEventMessage::ACS_EVENT_FAILED},

        {"DFS-RADAR-DETECTED", WpaEventMessage::DFS_EVENT_RADAR_DETECTED},
        {"DFS-NEW-CHANNEL", WpaEventMessage::DFS_EVENT_NEW_CHANNEL},
        {"DFS-CAC-START", WpaEventMessage::DFS_EVENT_CAC_START},
        {"DFS-CAC-COMPLETED", WpaEventMessage::DFS_EVENT_CAC_COMPLETED},
        {"DFS-NOP-FINISHED", WpaEventMessage::DFS_EVENT_NOP_FINISHED},
        {"DFS-PRE-CAC-EXPIRED", WpaEventMessage::DFS_EVENT_PRE_CAC_EXPIRED},

        {"AP-CSA-FINISHED", WpaEventMessage::AP_CSA_FINISHED},

        {"P2P-LISTEN-OFFLOAD-STOPPED", WpaEventMessage::P2P_EVENT_LISTEN_OFFLOAD_STOP},
        {"P2P-LISTEN-OFFLOAD-STOP-REASON", WpaEventMessage::P2P_LISTEN_OFFLOAD_STOP_REASON},

        /* BSS Transition Management Response frame received */
        {"BSS-TM-RESP", WpaEventMessage::BSS_TM_RESP},

        /* Collocated Interference Request frame received;
         * parameters: <dialog token> <automatic report enabled> <report timeout> */
        {"COLOC-INTF-REQ", WpaEventMessage::COLOC_INTF_REQ},
        /* Collocated Interference Report frame received;
         * parameters: <STA address> <dialog token> <hexdump of report elements> */
        {"COLOC-INTF-REPORT", WpaEventMessage::COLOC_INTF_REPORT},

        /* MBO IE with cellular data connection preference received */
        {"MBO-CELL-PREFERENCE", WpaEventMessage::MBO_CELL_PREFERENCE},

        /* BSS Transition Management Request received with MBO transition reason */
        {"MBO-TRANSITION-REASON", WpaEventMessage::MBO_TRANSITION_REASON},

        /* parameters: <STA address> <dialog token> <ack=0/1> */
        {"BEACON-REQ-TX-STATUS", WpaEventMessage::BEACON_REQ_TX_STATUS},
        /* parameters: <STA address> <dialog token> <report mode> <beacon report> */
        {"BEACON-RESP-RX", WpaEventMessage::BEACON_RESP_RX},

        /* PMKSA cache entry added; parameters: <BSSID> <network_id> */
        {"PMKSA-CACHE-ADDED", WpaEventMessage::PMKSA_CACHE_ADDED},
        /* PMKSA cache entry removed; parameters: <BSSID> <network_id> */
        {"PMKSA-CACHE-REMOVED", WpaEventMessage::PMKSA_CACHE_REMOVED},

        /* FILS HLP Container receive; parameters: dst=<addr> src=<addr> frame=<hexdump>
         */
        {"FILS-HLP-RX", WpaEventMessage::FILS_HLP_RX},

        /* Event to indicate Probe Request frame;
         * parameters: sa=<STA MAC address> signal=<signal> */
        {"RX-PROBE-REQUEST", WpaEventMessage::RX_PROBE_REQUEST},

        /* Event to indicate station's HT/VHT operation mode change information */
        {"STA-OPMODE-MAX-BW-CHANGED", WpaEventMessage::STA_OPMODE_MAX_BW_CHANGED},
        {"STA-OPMODE-SMPS-MODE-CHANGED", WpaEventMessage::STA_OPMODE_SMPS_MODE_CHANGED},
        {"STA-OPMODE-N_SS-CHANGED", WpaEventMessage::STA_OPMODE_N_SS_CHANGED},

        /* New interface addition or removal for 4addr WDS SDA */
        {"WDS-STA-INTERFACE-ADDED", WpaEventMessage::WDS_STA_INTERFACE_ADDED},
        {"WDS-STA-INTERFACE-REMOVED", WpaEventMessage::WDS_STA_INTERFACE_REMOVED},

        /* Transition mode disabled indication - followed by bitmap */
        {"TRANSITION-DISABLE", WpaEventMessage::TRANSITION_DISABLE},

        /* OCV validation failure; parameters: addr=<src addr>
         * frame=<saqueryreq/saqueryresp> error=<error string> */
        {"OCV-FAILURE", WpaEventMessage::OCV_FAILURE},

        /* Event triggered for received management frame */
        {"AP-MGMT-FRAME-RECEIVED", WpaEventMessage::AP_MGMT_FRAME_RECEIVED},
}};

WpaEventMessage cotask::GetWpaEventMessage(const std::string &message)
{
    auto result = stringToWpaEventMessage.find(message);
    if (result == stringToWpaEventMessage.end())
    {
        return WpaEventMessage::WPA_UNKOWN_MESSAGE;
    }
    return result->second;
}