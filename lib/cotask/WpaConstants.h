
enum class SelectTimeoutSecs {
    Normal = 10,   // p2p find refreshes
    Connect = 90,   // p2p connect
    Long = 600,       // p2p find refreshes after exceeding self.max_scan_polling
    Enroller = 600     // Period used by the enroller.
};
