// constants you can use in your plugin


// constants for the 'code' parameter in the TabNotify callback
var TCN_SELCHANGE         = -551  // the active tab has changed
var TCN_SELCHANGING       = -552  // the active tab is about to change
var TCN_TABDROPPED        = 0x401 // tab is dropped to the active BP window
var TCN_TABDROPPEDOUTSIDE = 0x402 // tab is dropped outsife of BP
var TCN_TABDELETE         = 0x403 // a tab was deleted
var TCN_ORDERCHANGED      = 0x405 // the tab order was changed
var TCN_REFRESH           = 0x406 // tab is refreshed/reloaded
var TCN_GETCOLOR          = 0x407 // BP asks for the tab color
var TCN_RELOAD            = 0x409 // BP reloads an externally modified file
