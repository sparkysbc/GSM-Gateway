/*
 * liballogsmat: An implementation of Allo GSM cards
 *
 * Parts taken from libgsmat
 * Written by mark.liu <mark.liu@openvox.cn>
 *
 * $Id: compiler.h 60 2010-09-09 07:59:03Z liuyuan $
 *
 * This program is free software, distributed under the terms of
 * the GNU General Public License Version 2 as published by the
 * Free Software Foundation. See the LICENSE file included with
 * this program for more details.
 *
 */

#ifndef _ASTERISK_COMPILER_H
#define _ASTERISK_COMPILER_H

#if __GNUC__ < 2 || (__GNUC__ == 2 && __GNUC_MINOR__ < 96)
#define __builtin_expect(exp, c) (exp)
#endif

#endif /* _ASTERISK_COMPILER_H */

