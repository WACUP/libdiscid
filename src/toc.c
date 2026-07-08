/* --------------------------------------------------------------------------

   MusicBrainz -- The Internet music metadatabase

   Copyright (C) 2006 Lukas Lalinsky

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, see
   <https://www.gnu.org/licenses/>.

--------------------------------------------------------------------------- */

#ifdef _MSC_VER
	#define _CRT_SECURE_NO_WARNINGS
	#if (_MSC_VER < 1900)
		#define snprintf _snprintf
	#endif
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "discid/discid_private.h"

#define XA_INTERVAL		((60 + 90 + 2) * 75)
#define DATA_TRACK		0x04


int mb_disc_load_toc(mb_disc_private *disc, mb_disc_toc *toc)  {
	int first_audio_track, last_audio_track, i, data_tracks;
	mb_disc_toc_track *track;

	if (toc->first_track_num < 1) {
#ifdef NO_ERROR_MSG
		snprintf(disc->error_msg, MB_ERROR_MSG_LENGTH,
			"invalid CD TOC - first track number must be 1 or higher");
#endif
		return 0;
	}

	if (toc->last_track_num < 1) {
#ifdef NO_ERROR_MSG
		snprintf(disc->error_msg, MB_ERROR_MSG_LENGTH,
			"invalid CD TOC - last track number must be 99 or lower");
#endif
		return 0;
	}

	/* we can't just skip data tracks at the front
	 * releases are always expected to start with track 1 by MusicBrainz
	 */
	data_tracks = 0;
	first_audio_track = toc->first_track_num;
	last_audio_track = -1;
	/* scan the TOC for audio tracks */
	for (i = toc->first_track_num; i <= toc->last_track_num; i++) {
		track = &toc->tracks[i];
		if ( !(track->control & DATA_TRACK) ) {
			last_audio_track = i;
			disc->track_type[i] = 0;
		} else {
			++data_tracks;
			disc->track_type[i] = 1;
		}
	}

	// dro change - there's a weirdness with a mixed-mode CD image
	//				when using PowerISO where the audio tracks are
	//				being reported as data tracks which prevents
	//				them from then being accessible to play. so we
	//				will see if we've got multiple data tracks and
	//				if it matches the whole CD we'll treat them as
	//				if they're audio tracks instead to fool things
	if ( (data_tracks == toc->last_track_num) && (toc->last_track_num > 1))
	{
		last_audio_track = data_tracks;
	}

	if (last_audio_track < 0) {
#ifdef NO_ERROR_MSG
		snprintf(disc->error_msg, MB_ERROR_MSG_LENGTH,
			"no actual audio tracks on disc: CDROM or DVD?");
#endif
		return 0;
	}

	disc->first_track_num = first_audio_track;
	disc->last_track_num = last_audio_track;

	/* get offsets for all found data tracks */
	for (i = first_audio_track; i <= last_audio_track; i++) {
		track = &toc->tracks[i];
		if (track->address > 0) {
			disc->track_offsets[i] = track->address + 150;
		} else {
			/* this seems to happen on "copy-protected" discs */
			disc->track_offsets[i] = 150;
		}
	}

	/* if the last audio track is not the last track on the CD,
	 * use the offset of the next data track as the "lead-out" offset */
	if (last_audio_track < toc->last_track_num) {
		track = &toc->tracks[last_audio_track + 1];
		disc->track_offsets[0] = track->address - XA_INTERVAL + 150;
	} else {
		/* use the regular lead-out track */
		track = &toc->tracks[0];
		disc->track_offsets[0] = track->address + 150;
	}

	/* as long as the lead-out isn't actually bigger than
	 * the position of the last track, the last track is invalid.
	 * This happens on "copy-protected"/invalid discs.
	 * The track is then neither a valid audio track, nor data track.
	 */
	while (disc->track_offsets[0] < disc->track_offsets[last_audio_track]) {
		disc->last_track_num = --last_audio_track;
		disc->track_offsets[last_audio_track + 1] = 0;
		track = &toc->tracks[last_audio_track + 1];
		disc->track_offsets[0] = track->address - XA_INTERVAL + 150;
	}

	return 1;
}

/* EOF */
