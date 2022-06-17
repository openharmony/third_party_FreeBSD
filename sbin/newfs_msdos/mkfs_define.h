/*
 * Copyright (C) 2022 Huawei Device Co., Ltd.
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef MKFS_DEFINE_H
#define MKFS_DEFINE_H
#include <stdint.h>

/*
 * BSD_NPARTS_MIN,BSD_NDRIVEDATA,BSD_NSPARE
 * Link： https://github.com/freebsd/freebsd-src/
 * Path:  freebsd-src/sys/sys/disk/bsd.h
 */
#define	BSD_NPARTS_MIN		8

/* Drive-type specific data size (in number of 32-bit inegrals) */
#define	BSD_NDRIVEDATA		5

/* Number of spare 32-bit integrals following drive-type data */
#define	BSD_NSPARE		5

/*
 * MAXPHYS
 * Link： https://github.com/freebsd/freebsd-src/
 * Path:  freebsd-src/sys/sys/param.h
 */
#ifdef __ILP32__
#define MAXPHYS		(128 * 1024)
#else
#define MAXPHYS		(1024 * 1024)
#endif

/*
 * DIOCGMEDIASIZE,DIOCGFWSECTORS,DIOCGSECTORSIZE,DIOCGFWHEADS
 * Link： https://github.com/freebsd/freebsd-src/
 * Path:  freebsd-src/sys/sys/disk.h
 */
#define	DIOCGMEDIASIZE	_IOR('d', 129, off_t)	/* Get media size in bytes */
	/*
	 * Get the size of the entire device in bytes.  This should be a
	 * multiple of the sector size.
	 */

#define	DIOCGFWSECTORS	_IOR('d', 130, u_int)	/* Get firmware's sectorcount */
	/*
	 * Get the firmware's notion of number of sectors per track.  This
	 * value is mostly used for compatibility with various ill designed
	 * disk label formats.  Don't use it unless you have to.
	 */

#define	DIOCGSECTORSIZE	_IOR('d', 128, u_int)
	/*
	 * Get the sector size of the device in bytes.  The sector size is the
	 * smallest unit of data which can be transferred from this device.
	 * Usually this is a power of 2 but it might not be (i.e. CDROM audio).
	 */

#define	DIOCGFWHEADS	_IOR('d', 131, u_int)	/* Get firmware's headcount */
	/*
	 * Get the firmwares notion of number of heads per cylinder.  This
	 * value is mostly used for compatibility with various ill designed
	 * disk label formats.  Don't use it unless you have to.
	 */

/*
 * FD_GTYPE
 * Link： https://github.com/freebsd/freebsd-src/
 * Path:  freebsd-src/sys/sys/fdcio.h
 */
#ifndef	_MACHINE_IOCTL_FD_H_
#define FD_GTYPE  _IOR('F', 62, struct fd_type)  /* get drive type */
#endif

/*
 * disklabel
 * Link： https://github.com/freebsd/freebsd-src/
 * Path:  freebsd-src/sys/sys/disk/bsd.h
 */
struct disklabel {
	uint32_t d_magic;		/* the magic number */
	uint16_t d_type;		/* drive type */
	uint16_t d_subtype;		/* controller/d_type specific */
	char	 d_typename[16];	/* type name, e.g. "eagle" */

	char	 d_packname[16];	/* pack identifier */

	/* disk geometry: */
	uint32_t d_secsize;		/* # of bytes per sector */
	uint32_t d_nsectors;		/* # of data sectors per track */
	uint32_t d_ntracks;		/* # of tracks per cylinder */
	uint32_t d_ncylinders;		/* # of data cylinders per unit */
	uint32_t d_secpercyl;		/* # of data sectors per cylinder */
	uint32_t d_secperunit;		/* # of data sectors per unit */

	/*
	 * Spares (bad sector replacements) below are not counted in
	 * d_nsectors or d_secpercyl.  Spare sectors are assumed to
	 * be physical sectors which occupy space at the end of each
	 * track and/or cylinder.
	 */
	uint16_t d_sparespertrack;	/* # of spare sectors per track */
	uint16_t d_sparespercyl;	/* # of spare sectors per cylinder */
	/*
	 * Alternate cylinders include maintenance, replacement, configuration
	 * description areas, etc.
	 */
	uint32_t d_acylinders;		/* # of alt. cylinders per unit */

	/* hardware characteristics: */
	/*
	 * d_interleave, d_trackskew and d_cylskew describe perturbations
	 * in the media format used to compensate for a slow controller.
	 * Interleave is physical sector interleave, set up by the
	 * formatter or controller when formatting.  When interleaving is
	 * in use, logically adjacent sectors are not physically
	 * contiguous, but instead are separated by some number of
	 * sectors.  It is specified as the ratio of physical sectors
	 * traversed per logical sector.  Thus an interleave of 1:1
	 * implies contiguous layout, while 2:1 implies that logical
	 * sector 0 is separated by one sector from logical sector 1.
	 * d_trackskew is the offset of sector 0 on track N relative to
	 * sector 0 on track N-1 on the same cylinder.  Finally, d_cylskew
	 * is the offset of sector 0 on cylinder N relative to sector 0
	 * on cylinder N-1.
	 */
	uint16_t d_rpm;			/* rotational speed */
	uint16_t d_interleave;		/* hardware sector interleave */
	uint16_t d_trackskew;		/* sector 0 skew, per track */
	uint16_t d_cylskew;		/* sector 0 skew, per cylinder */
	uint32_t d_headswitch;		/* head switch time, usec */
	uint32_t d_trkseek;		/* track-to-track seek, usec */
	uint32_t d_flags;		/* generic flags */
	uint32_t d_drivedata[BSD_NDRIVEDATA];	/* drive-type specific data */
	uint32_t d_spare[BSD_NSPARE];	/* reserved for future use */
	uint32_t d_magic2;		/* the magic number (again) */
	uint16_t d_checksum;		/* xor of data incl. partitions */

	/* filesystem and partition information: */
	uint16_t d_npartitions;	/* number of partitions in following */
	uint32_t d_bbsize;		/* size of boot area at sn0, bytes */
	uint32_t d_sbsize;		/* max size of fs superblock, bytes */
	struct partition {		/* the partition table */
		uint32_t p_size;	/* number of sectors in partition */
		uint32_t p_offset;	/* starting sector */
		uint32_t p_fsize;	/* filesystem basic fragment size */
		uint8_t  p_fstype;	/* filesystem type, see below */
		uint8_t  p_frag;	/* filesystem fragments per block */
		uint16_t p_cpg;		/* filesystem cylinders per group */
	} d_partitions[BSD_NPARTS_MIN];	/* actually may be more */
};

/*
 * fd_type
 * Link： https://github.com/freebsd/freebsd-src/
 * Path： freebsd-src/sys/sys/fdcio.h
 */
struct fd_type {
	int	sectrac;		/* sectors per track         */
	int	secsize;		/* size code for sectors     */
	int	datalen;		/* data len when secsize = 0 */
	int	gap;			/* gap len between sectors   */
	int	tracks;			/* total number of cylinders */
	int	size;			/* size of disk in sectors   */
	int	trans;			/* transfer speed code       */
	int	heads;			/* number of heads	     */
	int     f_gap;                  /* format gap len            */
	int     f_inter;                /* format interleave factor  */
	int	offset_side2;		/* offset of sectors on side2 */
	int	flags;			/* misc. features */
#define FL_MFM		0x0001		/* MFM recording */
#define FL_2STEP	0x0002		/* 2 steps between cylinders */
#define FL_PERPND	0x0004		/* perpendicular recording */
#define FL_AUTO		0x0008		/* autodetect format */
};
#endif //MKFS_DEFINE_H