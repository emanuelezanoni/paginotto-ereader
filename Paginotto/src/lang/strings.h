#ifndef PAGINOTTO_STRINGS_H
#define PAGINOTTO_STRINGS_H

// ============================================================================
//  Paginotto string table.
// ============================================================================

// ----------------------------------------------------------------------------
//  Boot / fatal screens (Paginotto.ino)
// ----------------------------------------------------------------------------
#define D_BOOT_STORAGE_ERROR        "Storage error"
#define D_BOOT_TRY_FACTORY_RESET    "Try factory reset"

// ----------------------------------------------------------------------------
//  Library screen — section title + system menu entries
//  (src/ui/screens/library_screen.cpp). The "+ " / "- " expansion indicators
//  in entryLabel() are visual symbols and intentionally NOT translated.
// ----------------------------------------------------------------------------
#define D_MENU_WEB_UI               "Open WebUI"
#define D_LIBRARY_OPEN_FAILED       "Open failed"
#define D_LIBRARY_TRY_UPLOAD        "Try WebUI again"

// ----------------------------------------------------------------------------
//  Upload screen (src/ui/screens/upload_screen.cpp)
// ----------------------------------------------------------------------------
#define D_UPLOAD_HEADER             "WebUI"
#define D_UPLOAD_WIFI               "Wi-Fi"
#define D_UPLOAD_PASSWORD           "Password"
#define D_UPLOAD_OPEN               "Open"
#define D_UPLOAD_CONNECTING         "Connecting..."
#define D_UPLOAD_CONNECTED          "Connected"
#define D_UPLOAD_HOTSPOT_HINT_L1    "Press button to"
#define D_UPLOAD_HOTSPOT_HINT_L2    "use hotspot instead"
#define D_UPLOAD_SEARCHING_CONFIGURED "Looking for saved networks..."
#define D_UPLOAD_NO_CONFIGURED      "No saved networks configured"
#define D_UPLOAD_NONE_VISIBLE       "No saved networks visible"
#define D_UPLOAD_WAITING_NETWORK    "No networks found yet - please wait"
#define D_UPLOAD_SELECT_NETWORK     "Saved network"
#define D_UPLOAD_SELECT_HINT        "Tap: select  2x: next"
#define D_UPLOAD_CONN_FAILED        "Connection failed. Resuming search..."
#define D_UPLOAD_HOTSPOT_OPTION     "Use hotspot (no Wi-Fi)"
#define D_UPLOAD_SCAN_TIMED_OUT     "Search timed out"
#define D_UPLOAD_SCAN_COMPLETE      "Search complete"

// ----------------------------------------------------------------------------
//  Update screen (src/ui/screens/update_screen.cpp)
// ----------------------------------------------------------------------------
#define D_MENU_UPDATE               "Firmware Update"
#define D_UPDATE_HEADER             "Firmware Update"
#define D_UPDATE_VERSION_PREFIX     "Current Version: "
#define D_UPDATE_WIFI_HEADER        "Select Wi-Fi network"
#define D_UPDATE_WIFI_SEARCHING     "Searching saved networks..."
#define D_UPDATE_WIFI_NONE          "No saved Wi-Fi networks"
#define D_UPDATE_WIFI_NOT_FOUND     "No Wi-Fi network found yet"
#define D_UPDATE_WIFI_HINT          "2x connect  3x back"
#define D_UPDATE_CHANNEL_LABEL      "Choose update channel"
#define D_UPDATE_CHAN_STABLE        "Stable"
#define D_UPDATE_CHAN_DEV           "Dev"
#define D_UPDATE_BTN_CHECK          "[ Check for update ]"
#define D_UPDATE_NO_CREDS_L1        "No Wi-Fi credentials."
#define D_UPDATE_NO_CREDS_L2        "Connect Wi-Fi via WebUI first."
#define D_UPDATE_CONNECTING         "Connecting..."
#define D_UPDATE_CONN_FAILED        "Wi-Fi connection failed"
#define D_UPDATE_CHECKING           "Checking..."
#define D_UPDATE_SERVER_FAIL        "Cannot reach update server"
#define D_UPDATE_STABLE_CURRENT     "Stable: up to date"
#define D_UPDATE_STABLE_AVAILABLE   "Stable: "
#define D_UPDATE_DEV_CURRENT        "Dev: up to date"
#define D_UPDATE_DEV_AVAILABLE      "Dev: update to commit "
#define D_UPDATE_BTN_INSTALL        "[ Install update ]"
#define D_UPDATE_INSTALLING         "Installing..."
#define D_UPDATE_DOWNLOAD_FAILED    "Download failed"
#define D_UPDATE_REBOOT_MSG         "Update installed"
#define D_UPDATE_REBOOT_HINT        "2x to reboot"

// ----------------------------------------------------------------------------
//  Reader (src/ui/reader.cpp)
// ----------------------------------------------------------------------------
#define D_READER_BOOK_EMPTY         "Book empty"
#define D_READER_BACK_LIBRARY       "Back to library"

// ----------------------------------------------------------------------------
//  Lock / screensaver (src/ui/sleep.cpp, Paginotto.ino)
// ----------------------------------------------------------------------------
#define D_SCREENSAVER_LOCKED        "Locked"

// ============================================================================
//  Web UI (captive portal) — strings embedded in HTML via adjacent-literal
//  concatenation. All endpoints declare Content-Type: charset=utf-8 already,
//  so accented characters survive transit unchanged.
// ============================================================================

// ----------------------------------------------------------------------------
//  Shared chrome / storage card (src/web/chrome.{h,cpp})
// ----------------------------------------------------------------------------
#define D_WEB_STORAGE_HEADING       "Storage"
#define D_WEB_STORAGE_BOOKS         "Books"
#define D_WEB_STORAGE_USED          "Used"
#define D_WEB_STORAGE_FREE          "Free"
#define D_WEB_STORAGE_TOTAL         "Total"
#define D_WEB_STORAGE_PCT_SUFFIX    "% of internal storage currently used."

// ----------------------------------------------------------------------------
//  Navigation links (used across multiple route handlers)
// ----------------------------------------------------------------------------
#define D_WEB_NAV_BOOKS             "Books"
#define D_WEB_NAV_SCREENSAVER       "Screensavers"
#define D_WEB_NAV_SETTINGS          "Settings"

// ----------------------------------------------------------------------------
//  Book upload (src/web/files.cpp handleFiles)
// ----------------------------------------------------------------------------
#define D_WEB_UPLOAD_BOOK_HEADING   "Upload book"
#define D_WEB_UPLOAD_BOOK_DESC      "Send UTF-8 plain text books or EPUBs to <b>/books</b>. EPUBs convert to plain text in this browser before upload; the device stores only .txt files."
#define D_WEB_UPLOAD_BOOK_BUTTON    "Upload"
#define D_WEB_MANAGE_BOOKS_BUTTON   "Manage books"
#define D_WEB_NOTES_HEADING         "Notes"
#define D_WEB_NOTES_DESC            "Uploaded books are normalized and compacted before saving, so a source TXT can be larger than the final stored file. The reader is optimized for UTF-8 plain text."

// ----------------------------------------------------------------------------
//  Books page (src/web/files.cpp handleFiles + folder/move/jump forms)
// ----------------------------------------------------------------------------
#define D_WEB_BOOKS_HEADING         "Books"
#define D_WEB_BOOKS_SUBTITLE        "Manage books, folders and library structure for Paginotto."
#define D_WEB_BOOKS_PATHS_DESC      "Books are stored as UTF-8 .txt files under /books. Only one folder level is supported; leaving Move blank returns a book to the library root."
#define D_WEB_CREATE_FOLDER_HEADING "Create folder"
#define D_WEB_CREATE_FOLDER_PLACEHOLDER "folder name"
#define D_WEB_CREATE_FOLDER_BUTTON  "Create folder"
#define D_WEB_CREATE_FOLDER_HINT    "One level only; maximum 24 characters."
#define D_WEB_FOLDERS_HEADING       "Library tree"
#define D_WEB_NO_FOLDERS            "No folders yet. Books currently live in the root of /books."
#define D_WEB_CONFIRM_DELETE_FOLDER "Delete folder? Only empty folders can be deleted."
#define D_WEB_DELETE_BUTTON         "Delete"
#define D_WEB_LIBRARY_BOOKS_HEADING "Library books"
#define D_WEB_LIBRARY_FULL_WARN     "&#9888; Library full (80 books max). Delete books to make room."
#define D_WEB_FOLDER_LIMIT_WARN     "&#9888; Folder limit reached (32 max)."
#define D_WEB_NO_BOOKS_UPLOADED     "No books uploaded yet."
#define D_WEB_BOOK_ROOT             "Root"
#define D_WEB_BOOK_BYTES_LABEL      " bytes"
#define D_WEB_BOOK_KB_LABEL         " KB"
#define D_WEB_BOOK_NAME_LIMIT_HINT  "The device shows 29 characters; longer titles appear as [...]"
#define D_WEB_BOOK_NAME_COUNTER     " characters"
#define D_WEB_FOLDER_NAME_COUNTER   " characters"
#define D_WEB_BOOK_FOLDER_LABEL     " | folder: "
#define D_WEB_BOOK_CURRENT_PAGE     " | current page: "
#define D_WEB_JUMP_BUTTON           "Jump"
#define D_WEB_JUMP_HINT             "Set the page that should open next on the device."
#define D_WEB_JUMP_HINT2            "The first open may take a moment."
#define D_WEB_PAGE_PLACEHOLDER      "Page"
#define D_WEB_MOVE_BUTTON           "Move"
#define D_WEB_MOVE_HINT             "Choose a top-level folder, or leave it in the library root."
#define D_WEB_MOVE_PLACEHOLDER      "leave blank for root"
#define D_WEB_RENAME_BUTTON         "Rename"
#define D_WEB_RENAME_HINT           "Keeps .txt and your reading progress."
#define D_WEB_RENAME_PLACEHOLDER    "Book name"
#define D_WEB_CONFIRM_DELETE_FILE   "Delete file?"
#define D_WEB_DOWNLOAD_BUTTON       "Download"

// ----------------------------------------------------------------------------
//  Plain-text 4xx/5xx error bodies (src/web/files.cpp and related routes).
//  These reach the browser on malformed requests; they
//  surface as page content if the user navigates a bad URL.
// ----------------------------------------------------------------------------
#define D_WEB_ERR_MISSING_ID            "missing id"
#define D_WEB_ERR_BAD_ID                "bad id"
#define D_WEB_ERR_MISSING_FOLDER        "missing folder"
#define D_WEB_ERR_BAD_FOLDER            "bad folder"
#define D_WEB_ERR_FOLDER_LIMIT          "folder limit reached"
#define D_WEB_ERR_MKDIR_FAILED          "mkdir failed"
#define D_WEB_ERR_FOLDER_NOT_FOUND      "folder not found"
#define D_WEB_ERR_FOLDER_NOT_EMPTY      "folder not empty"
#define D_WEB_ERR_DELETE_FAILED         "delete failed"
#define D_WEB_ERR_FOLDER_CREATE_FAILED  "folder create failed"
#define D_WEB_ERR_DEST_EXISTS           "destination exists"
#define D_WEB_ERR_MOVE_FAILED           "move failed"
#define D_WEB_ERR_MISSING_NAME           "missing name"
#define D_WEB_ERR_BAD_NAME               "bad name"
#define D_WEB_ERR_RENAME_FAILED          "rename failed"
#define D_WEB_ERR_MISSING_ID_PAGE       "missing id/page"

// ----------------------------------------------------------------------------
//  Reset page (src/web/reset.cpp)
// ----------------------------------------------------------------------------
#define D_WEB_RESET_HEADING         "Factory Reset"
#define D_WEB_RESET_SUBTITLE        "Erase all books, progress, and custom assets."
#define D_WEB_RESET_CONFIRM_HEADING "Confirm reset"
#define D_WEB_RESET_WARNING         "This will delete ALL books and reading progress."
#define D_WEB_RESET_DETAIL          "The device filesystem will be formatted and settings will return to defaults."
#define D_WEB_RESET_YES_BUTTON      "Yes, reset"
#define D_WEB_RESET_CONFIRM_MESSAGE "Factory reset? This deletes all books, progress, settings, and custom assets."
#define D_WEB_RESET_COMPLETE_HEADING "Factory reset complete"
#define D_WEB_RESET_COMPLETE_DESC   "All books, progress and custom assets were removed. The device is now back to a clean state."
#define D_WEB_RESET_SUCCESS_TITLE   "Reset complete"
#define D_WEB_RESET_SUCCESS_SUBTITLE "Paginotto was reset successfully."
#define D_WEB_RESET_BANNER          "&#10003; Factory reset complete."

// ----------------------------------------------------------------------------
//  Settings page (src/web/settings.cpp)
// ----------------------------------------------------------------------------
#define D_WEB_SETTINGS_TITLE        "Settings"
#define D_WEB_SETTINGS_SUBTITLE     "Device and reading settings."
#define D_WEB_FIRMWARE_HEADING      "Firmware"
#define D_WEB_FIRMWARE_LABEL        "Paginotto firmware v3.0"
#define D_WEB_READING_HEADING       "Reading"
#define D_WEB_FONT_SIZE_LABEL       "Font size"
#define D_WEB_FONT_SIZE_8           "8px &mdash; tiny"
#define D_WEB_FONT_SIZE_10          "10px &mdash; small"
#define D_WEB_FONT_SIZE_12          "12px &mdash; medium"
#define D_WEB_FONT_SIZE_14          "14px &mdash; large"
#define D_WEB_FONT_SIZE_HINT        "Controls how many lines fit on each page."
#define D_WEB_SLEEP_AFTER_LABEL     "Sleep after"
#define D_WEB_SLEEP_30S             "30 seconds"
#define D_WEB_SLEEP_1M              "1 minute"
#define D_WEB_SLEEP_2M              "2 minutes"
#define D_WEB_SLEEP_5M              "5 minutes"
#define D_WEB_SLEEP_10M             "10 minutes"
#define D_WEB_SLEEP_30M             "30 minutes"
#define D_WEB_SLEEP_HINT            "Auto-sleep keeps battery draw low while idle."
#define D_WEB_SLEEP_HEADING         "Sleep"
#define D_WEB_LINE_SPACING_LABEL    "Line spacing"
#define D_WEB_LINE_SPACING_0        "0 px &mdash; compact"
#define D_WEB_LINE_SPACING_1        "1 px &mdash; normal"
#define D_WEB_LINE_SPACING_2        "2 px &mdash; relaxed"
#define D_WEB_LINE_SPACING_3        "3 px &mdash; loose"
#define D_WEB_LINE_SPACING_HINT     "A small change here can make text much easier to scan."
#define D_WEB_NO_SCREENSAVER_LABEL  "No-screensaver mode"
#define D_WEB_NO_SCREENSAVER_HINT   "Device still sleeps on the normal timer and refreshes the screen before going to sleep. Then it shows the last page of the book, and skips a full refresh on wake, so you can continue reading with a single click of the button without the interruption of a display refresh."
#define D_WEB_LOCK_ON_SLEEP_LABEL   "Lock on sleep"
#define D_WEB_LOCK_ON_SLEEP_HINT    "Automatically lock the device every time it goes to sleep. Click-hold to unlock on next wake."
#define D_WEB_SAVE_SLEEP_BUTTON     "Save sleep settings"
#define D_WEB_SAVE_SETTINGS_BUTTON  "Save settings"
#define D_WEB_SETTINGS_NO_EXTRAS    "No extra files, scripts, or fonts."
#define D_WEB_SCREENSAVER_HEADING   "Screensaver"
#define D_WEB_SCREENSAVER_SPECS     "Upload raw XBM bytes: <b>3904 bytes</b>, 250&times;122 px, 1-bit, LSB-first, 32 bytes per row."
#define D_WEB_SCREENSAVER_TIP       "Tip: use <a class='link' href='https://javl.github.io/image2cpp/' target='_blank'>image2cpp</a> with <b>Plain bytes</b>. Invert colors if needed."
#define D_WEB_SCREENSAVER_ACTIVE    "&#10003; Custom screensaver active."
#define D_WEB_CONFIRM_DEL_SCREENSAVER "Delete custom screensaver?"
#define D_WEB_SCREENSAVER_DEFAULT   "Using built-in screensaver."
#define D_WEB_SLEEP_IMAGE_LABEL     "Sleep image file"
#define D_WEB_SCREENSAVER_UPLOAD_BUTTON "Upload image"

// Buttons / remappable hold-gestures section.
#define D_WEB_BUTTONS_HEADING       "Buttons"
#define D_WEB_BUTTONS_HINT          "1 click = next, 2 = previous, 3 = home. The two holds below are remappable."
#define D_WEB_BUTTONS_CLICK_HOLD    "Click, then hold"
#define D_WEB_BUTTONS_HOLD          "Hold"
#define D_WEB_BUTTONS_SAVE          "Save buttons"
#define D_WEB_BUTTONS_LOCK_HINT     "If locked, repeat any hold gesture to unlock."
#define D_WEB_BUTTONS_ACTION_NONE     "None"
#define D_WEB_BUTTONS_ACTION_LOCK     "Lock device"
#define D_WEB_BACK_TO_BOOKS          "Back to books"
#define D_WEB_BACK_TO_SCREENSAVERS   "Back to screensavers"

// ----------------------------------------------------------------------------
//  Upload (book + sleep image) routes (src/web/upload.cpp)
// ----------------------------------------------------------------------------
#define D_WEB_UPLOAD_COMPLETE_HEADING "Upload complete"
#define D_WEB_UPLOAD_COMPLETE_DESC  "Your book is now stored on the device and available in the library."
#define D_WEB_UPLOAD_BOOK_LABEL     "Book"
#define D_WEB_UPLOAD_STORED_SIZE    "Stored size"
#define D_WEB_UPLOAD_BOOKS_NOW      "Books now"
#define D_WEB_UPLOAD_FREE_SPACE     "Free space"
#define D_WEB_UPLOAD_ANOTHER        "Upload another"
#define D_WEB_UPLOAD_BOOK_SAVED     "Book saved successfully."
#define D_WEB_UPLOAD_FINISHED       "&#10003; Upload finished."
#define D_WEB_UPLOAD_ERR_FALLBACK   "Upload failed"
#define D_WEB_ERR_LIBRARY_FULL      "Library full"
#define D_WEB_ERR_NOT_ENOUGH_SPACE  "Not enough free space"
#define D_WEB_ERR_CANT_CREATE_TEMP_BOOK "Cannot create temp upload file"
#define D_WEB_ERR_WRITE_FAILED      "Write failed (out of space?)"
#define D_WEB_ERR_FINALIZE_UPLOAD   "Failed to finalize upload"
#define D_WEB_ERR_EMPTY_UPLOAD      "Empty upload"
#define D_WEB_ERR_UPLOAD_ABORTED    "Upload aborted"
#define D_WEB_SLEEP_UPLOAD_ERR_FALLBACK "Sleep image upload failed"
#define D_WEB_SLEEP_UPLOAD_HEADING  "Screensaver updated"
#define D_WEB_SLEEP_UPLOAD_DESC     "Your custom sleep image was saved successfully and will be shown the next time the device goes to sleep."
#define D_WEB_BACK_TO_SETTINGS      "Back to settings"
#define D_WEB_SLEEP_UPLOAD_SUBTITLE "Screensaver saved successfully."
#define D_WEB_SLEEP_UPLOAD_BANNER   "&#10003; Custom sleep image uploaded."
#define D_WEB_SLEEP_ERR_TEMP        "Cannot create temp sleep file"
#define D_WEB_SLEEP_ERR_SIZE        "Sleep image must be exactly 3904 bytes"
#define D_WEB_SLEEP_ERR_SAVE        "Failed to save sleep image"
#define D_WEB_SLEEP_ERR_ABORTED     "Sleep image upload aborted"

// ----------------------------------------------------------------------------
//  In-browser reader + find/jump (src/web/find.cpp).
// ----------------------------------------------------------------------------
#define D_WEB_READ_TITLE            "Edit &amp; jump"
#define D_WEB_READ_SUBTITLE         "Browse, edit and position the book in your browser."
#define D_WEB_READ_BYTES_LABEL      "bytes"
#define D_WEB_READ_KB_LABEL         "KB"
#define D_WEB_READ_CURRENT_PAGE_LABEL "current page:"
#define D_WEB_READ_FIND_PLACEHOLDER "Find in book"
#define D_WEB_READ_FIND_ALL         "Find all"
#define D_WEB_READ_FIND_PREV        "Prev"
#define D_WEB_READ_FIND_NEXT        "Next"
#define D_WEB_READ_JUMP_HERE        "Jump to here"
#define D_WEB_READ_LOADING          "Loading book text..."
#define D_WEB_READ_PAGE_PLACEHOLDER "Page number"
#define D_WEB_READ_JUMP_PAGE        "Jump to page"
#define D_WEB_READ_JUMP_HINT        "Saves the next-open page directly."
#define D_WEB_READ_AND_FIND_LINK    "Edit &amp; jump"
#define D_WEB_READ_EDIT_TEXT        "Edit text"
#define D_WEB_READ_EDIT_WARNING     "Warning: saving replaces the book on the device immediately. There is no device backup. Download the current TXT first if you may need to recover it."
#define D_WEB_READ_DOWNLOAD_BACKUP  "Download current TXT"
#define D_WEB_READ_SAVE_EDIT        "Save &amp; repaginate"
#define D_WEB_READ_EDIT_CANCEL      "Cancel editing"

// ----------------------------------------------------------------------------
//  Bionic reading + reading-position retention
//  (src/web/settings.cpp). Layout-affecting settings; changes trigger the
//  reader to remap its byte-offset cursor under the new layout.
// ----------------------------------------------------------------------------
#define D_WEB_READING_INTRO         "Changing the font size, line spacing, or bionic mode keeps your place in the current book &mdash; the device re-flows pages around the byte you're reading and lands on the page that contains it."
#define D_WEB_BIONIC_LABEL          "Bionic reading"
#define D_WEB_BIONIC_HINT           "Bolds the leading characters of each word to help your eyes anchor."
#define D_WEB_SETTINGS_APPLY_HINT   "Changes apply to the next page render."

// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
//  Screensaver editor + multi-slot manager (src/web/screensavers.cpp).
//  JS-internal status / error strings emitted by the editor are NOT yet i18n'd;
//  they live inside the PROGMEM script block. Add D_WEB_SS_JS_* macros and a
//  data-attribute pass-through if/when that's wanted.
// ----------------------------------------------------------------------------
#define D_WEB_SS_TITLE              "Screensavers"
#define D_WEB_SS_SUBTITLE           "Custom sleep images, multi-slot rotation, and in-firmware bitmap editor."
#define D_WEB_SS_ROTATION_HEADING   "Rotation"
#define D_WEB_SS_ROTATION_INTRO     "Pick what shows on the e-ink each time the device sleeps. Cycle walks the populated slots in order; Shuffle picks at random without immediate repeats."
#define D_WEB_SS_MODE_LABEL         "Mode"
#define D_WEB_SS_MODE_SINGLE        "Single image only"
#define D_WEB_SS_MODE_CYCLE         "Cycle through slots"
#define D_WEB_SS_MODE_SHUFFLE       "Shuffle slots"
#define D_WEB_SS_SLOTS_POPULATED    "Populated slots: "
#define D_WEB_SS_SAVE_MODE          "Save mode"
#define D_WEB_SS_SLOTS_HEADING      "Rotation slots"
#define D_WEB_SS_SLOT_LABEL         "Slot"
#define D_WEB_SS_SLOT_EMPTY         "empty"
#define D_WEB_SS_CONFIRM_DEL_SLOT   "Delete this slot?"
#define D_WEB_SS_DOWNLOAD_ARIA      "Download screensaver"
#define D_WEB_SS_UPLOAD_ARIA        "Upload screensaver .bin"
#define D_WEB_SS_DELETE_ARIA        "Delete screensaver"
#define D_WEB_SS_ROTATE             "Rotate 90\u00b0"
#define D_WEB_SS_SINGLE_HEADING     "Single screensaver"
#define D_WEB_SS_SINGLE_ALT         "Single screensaver"
#define D_WEB_SS_CONFIRM_DEL_SINGLE "Delete the single screensaver?"
#define D_WEB_SS_NO_SINGLE          "No single screensaver uploaded. Upload via Editor."
#define D_WEB_SS_EDITOR_HEADING     "Editor"
#define D_WEB_SS_EDITOR_INTRO       "Drop an image into the editor, then upload it to a rotation slot or as the single legacy screensaver. All images render as 250&times;122 1-bit (3904 bytes)."
#define D_WEB_SS_SOURCE_IMAGE       "Source image"
#define D_WEB_SS_TOLERANCE          "Black tolerance"
#define D_WEB_SS_INVERT             "Invert black/white"
#define D_WEB_SS_PRECISE_CONTROL    "Precise control"
#define D_WEB_SS_ZOOM               "Zoom"
#define D_WEB_SS_MOVE_X             "Move X"
#define D_WEB_SS_MOVE_Y             "Move Y"
#define D_WEB_SS_PREVIEW_LABEL      "Preview (drag to move, pinch or scroll to zoom)"
#define D_WEB_SS_RESET_FIT          "Reset fit"
#define D_WEB_SS_NO_IMAGE           "No image loaded"
#define D_WEB_SS_SAVE_TO            "Save to"
#define D_WEB_SS_DST_SINGLE         "Single screensaver (/sleep.bin)"
#define D_WEB_SS_DST_AUTO_PREFIX    "Next free rotation slot (slot "
#define D_WEB_SS_DST_AUTO_SUFFIX    ")"
#define D_WEB_SS_DST_FULL           "(All rotation slots full)"
#define D_WEB_SS_DST_SLOT_PREFIX    "Rotation slot "
#define D_WEB_SS_DST_OVERWRITE      " (overwrite)"
#define D_WEB_SS_UPLOAD_EDITED      "Upload edited image"

#endif  // PAGINOTTO_STRINGS_H
