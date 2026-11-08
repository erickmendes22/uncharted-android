/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef BB_PAD_HOST_POLICY_H
#define BB_PAD_HOST_POLICY_H
/* BB_PAD_FILE was designed for deterministic scripted input, in which the
 * physical pad must be ignored. Android's native MotionEvent controls also
 * publish BB_PAD_FILE, but are interactive and must MERGE with SDL host
 * gamepads and keyboard. Keep explicit BB_PAD_HOST=1 as another override. */
static inline int bb_pad_scripted_only(const char *file,
                                       const char *host, const char *java_touch) {
    const int has_file = file && file[0];
    const int merge_host = (host && host[0]=='1') ||
                           (java_touch && java_touch[0]=='1');
    return has_file && !merge_host;
}
#endif
