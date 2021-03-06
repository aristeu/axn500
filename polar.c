/*
 * This file is part of axn500.
 *
 * axn500 is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * axn500 is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with axn500. If not, see <http://www.gnu.org/licenses/>.
 *
 * (C) Copyright 2010 Aristeu S. Rozanski F. <aris@ruivo.org>
 */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <getopt.h>
#include <linux/types.h>
#include <linux/socket.h>
#include <linux/irda.h>

static int axn500_debug;
#define dprintf(x...) do { \
		if (axn500_debug) { \
			printf(x); \
			fflush(stdout); \
		} \
	} while(0)
#if 0
Datagram socket - SOCK_DGRAM, IRDAPROTO_UNITDATA
	SeqPacket sockets provides a reliable, datagram oriented, full duplex connection between two sockets on top of IrLMP.  There is no guarantees that the data arrives in order and there is  no
	flow contol, however IrLAP retransmits lost packets.
	Datagram sockets preserve record boundaries. No fragmentation is provided, datagrams larger than the IrDA link MTU are truncated or discarded.


struct sockaddr_irda {
	sa_family_t sir_family;   /* AF_IRDA */
	__u8        sir_lsap_sel; /* LSAP selector */
	__u32       sir_addr;     /* Device address */
	char        sir_name[25]; /* Usually <service>:IrDA:TinyTP */
};
       sin_family is always set to AF_IRDA.  sir_lsap_sel is usually not used.  sir_addr is the address of the peer and optional (and that case the first peer discoverd will be used).   sir_name  is  the
       service name of the socket.
       #include <sys/types.h>          /* See NOTES */
       #include <sys/socket.h>

       int socket(int domain, int type, int protocol);
       dgram_s = socket(PF_INET, SOCK_DGRAM, IRDAPROTO_UNITDATA);

       int connect(int sockfd, const struct sockaddr *addr,
                   socklen_t addrlen);

struct irda_device_info {
	__u32       saddr;    /* Address of local interface */
	__u32       daddr;    /* Address of remote device */
	char        info[22]; /* Description */
	__u8        charset;  /* Charset used for description */
	__u8        hints[2]; /* Hint bits */
};                                                                                                                                                             

#endif

#define CHECK(s, hint) \
	if (hint & s) {\
		pos += snprintf(buff, size, "%s", #s); \
		size -= pos; \
	}

static char *version = VERSION;

static void irda_get_hints(struct irda_device_info *info, char *buff, int size)
{
	unsigned char hint0, hint1;
	int pos = 0;

	hint0 = info->hints[0];
	hint1 = info->hints[1];

	CHECK(HINT_PNP, hint0);
	CHECK(HINT_PDA, hint0);
	CHECK(HINT_COMPUTER, hint0);
	CHECK(HINT_PRINTER, hint0);
	CHECK(HINT_MODEM, hint0);
	CHECK(HINT_FAX, hint0);
	CHECK(HINT_LAN, hint0);
	CHECK(HINT_EXTENSION, hint0);

	CHECK(HINT_TELEPHONY, hint1);
	CHECK(HINT_FILE_SERVER, hint1);
	CHECK(HINT_COMM, hint1);
	CHECK(HINT_MESSAGE, hint1);
	CHECK(HINT_HTTP, hint1);
	CHECK(HINT_OBEX, hint1);
}

static int irda_discover_devices(int fd, struct sockaddr_irda *addr, int max_devices)
{
	struct irda_device_list *list;
	int i;
	socklen_t size;
	char buff[10], *tmp;

	size = sizeof(struct irda_device_list) + sizeof(struct irda_device_info) * max_devices;
	tmp = malloc(size);
	if (tmp == NULL)
		return -1;

	dprintf("Scanning...\n");
	if (getsockopt(fd, SOL_IRLMP, IRLMP_ENUMDEVICES, tmp, &size))
		return 1;

	list = (struct irda_device_list *)tmp;
	dprintf("Found %i devices:\n", list->len);
	for (i = 0; i < list->len; i++) {
 		irda_get_hints(&list->dev[i], buff, sizeof(buff));
		dprintf("saddr: %#x, daddr: %#x, charset: %i, hints: %s, desc: [%s]\n",
			list->dev[i].saddr, list->dev[i].daddr,
			list->dev[i].charset, buff,
			list->dev[i].info);
	}
	addr->sir_addr = list->dev[0].daddr;
	return 0;
}

struct axn500_time {
	unsigned char hour;
	unsigned char minute;
	unsigned char second;
};

struct axn500_date {
	unsigned char year;
	unsigned char month;
	unsigned char day;
};

struct axn500_limit {
	unsigned char lower;
	unsigned char upper;
};

struct axn500_marker {
	struct axn500_time time;
	struct axn500_time moment;;
};

struct axn500_entry {
	unsigned char hr;
	signed short altitude;
};

struct axn500_exercise {
	struct axn500_date date;
	struct axn500_time start_time;
	struct axn500_time duration;
	struct axn500_limit limits[3];
	struct axn500_marker markers[5];
	unsigned char max_hr;
	unsigned char avg_hr;
	unsigned char num_markers;
	unsigned short kcal;
	signed short min_alt;
	signed short max_alt;
	int entries;
	struct axn500_entry *data;	
};

struct axn500 {
	struct axn500_alarm {
		struct axn500_time time;
		char enabled;
		char desc[8];
	} alarms[3];
	struct axn500_reminder {
		struct axn500_date date;
		struct axn500_time time;
		char enabled;
		char desc[8];
	} reminders[5];
	struct axn500_time timezone[2];
	unsigned char enabled_timezone;
	unsigned char ampm;

	struct axn500_date date;

	struct axn500_settings {
		struct axn500_date bday;
		unsigned char height;

		unsigned short weight;
		unsigned short record_rate;

		unsigned char activity;
		unsigned char hrmax;
		unsigned char vomax;
		unsigned char sit_hr;

		unsigned char activity_button_sound;
		unsigned char intro_animations;
		unsigned char imperial;
		unsigned char declination;

		struct axn500_time countdown;

		unsigned char sex;

		unsigned char htouch;
	} settings;

	struct axn500_exercises {
		int num;
		struct axn500_exercise *exercise;
	} exercises;
};

enum {
	AXN500_CMD_GET_TIME = 0,
	AXN500_CMD_GET_REMINDER1,
	AXN500_CMD_GET_REMINDER2,
	AXN500_CMD_GET_REMINDER3,
	AXN500_CMD_GET_REMINDER4,
	AXN500_CMD_GET_REMINDER5,
	AXN500_CMD_GET_SETTINGS,
};

static char axn500_parse_byte(char byte, int capsonly)
{
	if (byte < 0xa)
		return '0' + byte;
	if (byte == 0xa)
		return ' ';
	if (byte <= 0x24)
		return (byte - 0x0b) + 'A';
	if (capsonly) {
		if (byte == 0x2f)
			return '!';
	}
	if (byte >= 0x25 && byte <= 0x3e)
		return (byte - 0x25) + 'a';
	switch(byte) {
		default:
			break;
	}
	/* FIXME */
	fprintf(stderr, "Unknown character: %#x\n",
		(unsigned char)byte);
	return '?';
}

static char axn500_parse_hex(char byte)
{
	/*
	 * some values are stored in hex, for example:
	 *	0x3605
	 * meaning 5h36min
	 */
	return ((byte >> 4) * 10) + (byte & 0x0f);
}

/*
get alarm format
cmd: 0x29
0                      7                       15                      23                      31
/----------------------/-----------------------/-----------------------/-----------------------/-----------------
28 1f 0c 09 00 42 05 36 07 01 21 00 10 22 11 00 01 0d 0b 0b 8d 8a 8a 8a 0b 16 0b 1c 17 af 8a 0b 16 0b 1c 17 0a af
-- --------    ----- ----- ----- ----- ----- -- -- -------------------- -------------------- --------------------
||    |          |     |     |     |     |   || ||     alarm1 desc         alarm2 desc            alarm3 desc
||    |          |     |     |     |     |   || ||
||    |          |     |     |     |     |   || ++- 0x80 = 12h clock, 0xX0 = timezone1, 0xX1 = timezone2
||    |          |     |     |     |     |   ++---- alarms enabled: 0, 0x1, 0x3, 0x7
||    |          |     |     |     |     +--------- alarm 3, mm/hh (hex = string)
||    |          |     |     |     +--------------- alarm 2, mm/hh (hex = string)
||    |          |     |     +--------------------- alarm 1, mm/hh (hex = string)
||    |          |     +--------------------------- timezone 2, mm/hh (hex = string)
||    |          +--------------------------------- timezone 1, mm/hh (hex = string)
||    +-------------------------------------------- date, dd/mm/YY (hex = value)
++------------------------------------------------- command code
strings:
	0x10 <- end
	0x0a <- space
	0x0b <- 'A'
*/
#define AXN500_DATE_OFFSET		1
#define AXN500_TIMEZONE1_OFFSET		5
#define AXN500_TIMEZONE2_OFFSET		7
#define AXN500_ALARM_MINUTE_OFFSET	9
#define AXN500_ALARM_ENABLED_OFFSET	15
#define AXN500_TIMEZONE_SET		16
#define AXN500_ALARM_DESC_OFFSET	17
static int axn500_parse_time_info(int cmd, struct axn500 *info, char *data)
{
	int i, j, pos, enabled;
	char byte, end;

	enabled = data[AXN500_ALARM_ENABLED_OFFSET]; 
	/* alarm times are stored with the nominal values but in hex */
	for (i = 0; i < 3; i++) {
		pos = AXN500_ALARM_MINUTE_OFFSET + i * 2;
		info->alarms[i].time.minute = axn500_parse_hex(data[pos]);
		info->alarms[i].time.hour = axn500_parse_hex(data[pos + 1]);
		info->alarms[i].enabled = (enabled & (1 << i))? 1 : 0;
		for (j = 0; j < 7; j++) {
			pos = AXN500_ALARM_DESC_OFFSET + (i * 7);
			byte = data[pos + j] & 0x7F;
			end = data[pos + j] & 0x80;
			info->alarms[i].desc[j] = axn500_parse_byte(byte, 1);
			if (end) {
				j++;
				break;
			}
		}
		info->alarms[i].desc[j] = 0;
	}
	/* parsing timezone info */
	pos = AXN500_TIMEZONE1_OFFSET; 
	info->timezone[0].minute = axn500_parse_hex(data[pos]);
	info->timezone[0].hour = axn500_parse_hex(data[pos + 1]);
	pos = AXN500_TIMEZONE2_OFFSET; 
	info->timezone[1].minute = axn500_parse_hex(data[pos]);
	info->timezone[1].hour = axn500_parse_hex(data[pos + 1]);
	pos = AXN500_TIMEZONE_SET;
	info->enabled_timezone = (data[pos] & 0x1)? 1:0;
	info->ampm = (data[pos] & 0x80)? 1:0;

	/* parsing date info */
	pos = AXN500_DATE_OFFSET;
	info->date.day = data[pos];
	info->date.month = data[pos + 1];
	info->date.year = data[pos + 2];

	return 0;
}

/*
reminder info
cmd: 0x3501, 0x3502, 0x3503, 0x3504, 0x3505
0                      7                       15                      23                      31
/----------------------/-----------------------/-----------------------/-----------------------/-----------------
35:0b:1c:13:9d:8a:8a:8a:00:00:10:02:06:04
   ^^ ^^ ^^ ^^ ^^ ^^ ^^ ^^ ^^ ^^ ^^ ^^ ^^
   |------ desc ------| en mm hh dd MM YY
*/
#define AXN500_REMINDER_DESC_OFFSET 1
#define AXN500_REMINDER_ENABLED_OFFSET 8
#define AXN500_REMINDER_MINUTE_OFFSET 9
#define AXN500_REMINDER_DAY_OFFSET 11
static int axn500_parse_reminder_info(int cmd, struct axn500 *info,
				      char *data)
{
	int i, which;
	char byte, end;

	which = cmd - AXN500_CMD_GET_REMINDER1;

	for (i = 0; i < 7; i++) {
		byte = data[AXN500_REMINDER_DESC_OFFSET + i] & 0x7F;
		end = data[AXN500_REMINDER_DESC_OFFSET + i] & 0x80;
		info->reminders[which].desc[i] = axn500_parse_byte(byte, 0);
		if (end) {
			i++;
			break;
		}
	}
	info->reminders[which].desc[i] = 0;

	i = AXN500_REMINDER_MINUTE_OFFSET;
	info->reminders[which].time.minute = axn500_parse_hex(data[i]);
	info->reminders[which].time.hour = axn500_parse_hex(data[i + 1]);

	i = AXN500_REMINDER_DAY_OFFSET;
	info->reminders[which].date.day = data[i];
	info->reminders[which].date.month = data[i + 1];
	info->reminders[which].date.year = data[i + 2];

	i = AXN500_REMINDER_ENABLED_OFFSET;
	info->reminders[which].enabled = data[i]? 1:0;

	return 0;
}

/*
 * 0                      7                       15                      23                      31
 * /----------------------/-----------------------/-----------------------/-----------------------/-----------------
 * 2a e3 00 b4 12 0c 4e 00 01 b4 2d 46 0c 3c 00 02 00 00 00 4c b4 50 a0 50 a0 00 00 20 00 20 80
 * -- ----- -- -------- -- -- -- -- -- -- -- -- -- -------- -------------------------------- --
 * ||  ||   ||    ||    || || || || || ||    || ||    ||                                     ++- 0x0F = declination
 * ||  ||   ||    ||    || || || || || ||    || ||    ++---- countdown ss:mm:hh (hex = string)
 * ||  ||   ||    ||    || || || || || ||    || ++---------- heart touch (0 = off, 1 = light, 2 = switch display,
 * ||  ||   ||    ||    || || || || || ||    ||                           3 = take lap)
 * ||  ||   ||    ||    || || || || || ||    ++------------- record rate (0 = 5s, 1 = 15s, 2 = 60s, 3 = 5min)
 * ||  ||   ||    ||    || || || || || ++------------------- 0x08 = activity button sound (1 = off)
 * ||  ||   ||    ||    || || || || ||                       0x04 = intro animations (1 = on)
 * ||  ||   ||    ||    || || || || ||                       0x02 = metric/imperial (1 = imperial)
 * ||  ||   ||    ||    || || || || ++---------------------- sit Heart Rate (hex = value)
 * ||  ||   ||    ||    || || || ++------------------------- VOmax (hex = value)
 * ||  ||   ||    ||    || || ++---------------------------- HRmax (hex = value)
 * ||  ||   ||    ||    || ++------------------------------- activity type (0 = low, 1 = medium, 2 = high,
 * ||  ||   ||    ||    ||                                                  3 = top)
 * ||  ||   ||    ||    ++---------------------------------- sex (0 = male, 1 = female)
 * ||  ||   ||    ++---------------------------------------- birthday (dd mm YY, hex = value)
 * ||  ||   ++---------------------------------------------- height (in cm, hex = value)
 * ||  ++--------------------------------------------------- weight in lb (22 11 = 0x1122lb, hex = value)
 * ++------------------------------------------------------- command code
 */

#define AXN500_SETTINGS_WEIGHT_OFFSET		1
#define AXN500_SETTINGS_HEIGHT_OFFSET		3
#define AXN500_SETTINGS_BDAY_OFFSET		4

#define AXN500_SETTINGS_SEX_OFFSET		7
#define AXN500_SETTINGS_SEX_MALE		0
#define AXN500_SETTINGS_SEX_FEMALE		1

#define AXN500_SETTINGS_ACTIVITY_OFFSET		8
#define AXN500_SETTINGS_ACTIVITY_LOW		0
#define AXN500_SETTINGS_ACTIVITY_MEDIUM		1
#define AXN500_SETTINGS_ACTIVITY_HIGH		2
#define AXN500_SETTINGS_ACTIVITY_TOP		3

#define AXN500_SETTINGS_HRMAX_OFFSET		9
#define AXN500_SETTINGS_VOMAX_OFFSET		10
#define AXN500_SETTINGS_SITHR_OFFSET		11
#define AXN500_SETTINGS_MISC_OFFSET		12
#define AXN500_SETTINGS_RECRATE_OFFSET		14

#define AXN500_SETTINGS_HTOUCH_OFFSET		15
#define AXN500_SETTINGS_HTOUCH_OFF		0
#define AXN500_SETTINGS_HTOUCH_LIGHT		1
#define AXN500_SETTINGS_HTOUCH_SWITCH_DISPLAY	2
#define AXN500_SETTINGS_HTOUCH_TAKE_LAP		3

#define AXN500_SETTINGS_COUNTDOWN_OFFSET	16
#define AXN500_SETTINGS_DECLINATION_OFFSET	30
static int axn500_parse_settings(int cmd, struct axn500 *info, char *data)
{
	int pos;
	unsigned short *weight;

	pos = AXN500_SETTINGS_WEIGHT_OFFSET;
	weight = (unsigned short *)&data[pos];
	info->settings.weight = le16toh(*weight);

	pos = AXN500_SETTINGS_HEIGHT_OFFSET;
	info->settings.height = data[pos];

	pos = AXN500_SETTINGS_BDAY_OFFSET;
	info->settings.bday.day = data[pos];
	info->settings.bday.month = data[pos + 1];
	info->settings.bday.year = data[pos + 2];

	pos = AXN500_SETTINGS_SEX_OFFSET;
	info->settings.sex = data[pos];

	pos = AXN500_SETTINGS_ACTIVITY_OFFSET;
	info->settings.activity = data[pos];

	pos = AXN500_SETTINGS_HRMAX_OFFSET;
	info->settings.hrmax = data[pos];

	pos = AXN500_SETTINGS_VOMAX_OFFSET;
	info->settings.vomax = data[pos];

	pos = AXN500_SETTINGS_SITHR_OFFSET;
	info->settings.sit_hr = data[pos];

	pos = AXN500_SETTINGS_MISC_OFFSET;
	info->settings.activity_button_sound = (data[pos] & 0x08)? 0:1;
	info->settings.intro_animations = (data[pos] & 0x04)? 1:0;
	info->settings.imperial = (data[pos] & 0x02)? 1:0;

	pos = AXN500_SETTINGS_RECRATE_OFFSET;
	switch(data[pos]) {
		case 0:
			info->settings.record_rate = 5;
			break;
		case 1:
			info->settings.record_rate = 15;
			break;
		case 2:
			info->settings.record_rate = 60;
			break;
		case 3:
			info->settings.record_rate = 300;
			break;
		default:
			fprintf(stderr, "Strange record rate: %#x\n", data[pos]);
			break;
	}

	pos = AXN500_SETTINGS_HTOUCH_OFFSET;
	info->settings.htouch = data[pos];

	pos = AXN500_SETTINGS_COUNTDOWN_OFFSET;
	info->settings.countdown.second = axn500_parse_hex(data[pos]);
	info->settings.countdown.minute = axn500_parse_hex(data[pos + 1]);
	info->settings.countdown.second = axn500_parse_hex(data[pos + 2]);

	pos = AXN500_SETTINGS_DECLINATION_OFFSET;
	info->settings.declination = data[pos] & 0x7f;

	return 0;
}

/*
 * unknown commands
 * command		reply size
 * 0x2E			52
 * 0x30			24
 * 0x2D00		10
 * 0x2D01		10
 * 0x2D02		10
 * 0x2D03		10
 * 0x2D04		10
 * 0x2D05		10
 * 0x2D06		10
 * 0x2D07		10
 * 0x2D08		10
 * 0x2D09		10
 * 0x2D0A		10
 * 0x2D0B		10
 * 0x2D0C		10
 * 0x2D0D		10
 * 0x2D0E		10
 * 0x2D0F		10
 * 0x2D10		10
 * 0x2D11		10
 * 0x2D12		10
 * 0x2D13		10
 * 0x3373		48
 * 0x3300		48
 */
struct {
	char cmd[5];
	char cmdsize;
	int datasize;
	int (*parser)(int cmd, struct axn500 *info, char *data);
	int (*get_raw)(int cmd, struct axn500 *info, char *raw, int raw_size);
} axn500_commands[] = {
	[AXN500_CMD_GET_TIME] = { {0x29,}, 1, 38, axn500_parse_time_info, NULL},
	[AXN500_CMD_GET_REMINDER1] = { {0x35, 0x01,}, 2, 14, axn500_parse_reminder_info, NULL},
	[AXN500_CMD_GET_REMINDER2] = { {0x35, 0x02,}, 2, 14, axn500_parse_reminder_info, NULL},
	[AXN500_CMD_GET_REMINDER3] = { {0x35, 0x03,}, 2, 14, axn500_parse_reminder_info, NULL},
	[AXN500_CMD_GET_REMINDER4] = { {0x35, 0x04,}, 2, 14, axn500_parse_reminder_info, NULL},
	[AXN500_CMD_GET_REMINDER5] = { {0x35, 0x05,}, 2, 14, axn500_parse_reminder_info, NULL},
	[AXN500_CMD_GET_SETTINGS] = { {0x2b,}, 1, 31, axn500_parse_settings, NULL},
	{},
};

static void dump_context(char *ptr, int offset, int context)
{
	int i;
	unsigned char tmp;
	fprintf(stderr, "context: ");
	for (i = offset - context; i < (offset + context); i++) {
		tmp = ptr[i];
		if (i == offset)
			fprintf(stderr, "[%hhx] ", tmp);
		else
			fprintf(stderr, "%hhx ", tmp);
	}
	fprintf(stderr, "\n");
}

/*
 * Exercise packaging:
 * | preamble | exercise 1 info | data | exercise 2 info | data  ...
 * /--- 10 ---/------ ?? -------/....../
 *
 * So far, I can't determine the exercise info size, but it's possible to
 * use 00 00 00 c1 01 as pattern to know the exercise info is over
 *
 * Exercise info format:
 *	      55 05 ec 0c 0c 15
 *	                  ^^
 *	                  exercise day
 *	26 05 20 0c 38 00 01 00
 *	^^ ^^ ^^          ^^
 *	|| || ||          number of markers (at least one)
 *	ss mm hh (start time)
 *	
 *	12 29 01 7f bb 13 00 19
 *	^^ ^^ ^^ ^^ ^^
 *	|| || || || max
 *	|| || || average
 *	|| || hh duration
 *	|| mm duration
 *	ss duration
 *	00 05 00 05 00 07 03 fa
 *	               ^^^^^ ^^
 *	                ||   alt min
 *	                alt max
 *	02 1c 19 23 00 32 00 00
 *	^^       ^^
 *	         trip point 2
 *	5f b4 00 00 00 00 24 00
 *	^^ ^^ ^^ ^^ ^^ ^^
 *	|| || || || || upper3    \
 *	|| || || || lower3       |
 *	|| || || upper2          | upper/lower limits
 *	|| || lower2             |
 *	|| upper1                |
 *	lower1                   /
 *	00 52 27 01 56 00 00 00
 *	00 00 00 00 00 00 00 00
 *	00 00 00 00 00 00 00 00
 *	00 09 04 03 0c 15 05 20
 *	00 00 00 6b 02 03 b0 2f
 *	1c 00 00 00 00 00 00 c1
 *	01
 */
#define EX_DAY_OFFSET		4
#define EX_START_TIME_OFFSET	6
#define EX_MARKERNUM_OFFSET	12
#define EX_DURATION_OFFSET	14
#define EX_AVG_HR_OFFSET	17
#define EX_MAX_HR_OFFSET	18
#define EX_MAX_ALT_OFFSET	27
#define EX_MIN_ALT_OFFSET	29
#define EX_LIMITS_OFFSET	38
#define EX_KCAL_OFFSET		71

#define EX_HEADER_SIZE		95
#define EX_MARKER_SIZE		22
static int axn500_parse_exercises(char *data, int num_ex, int bytes, struct axn500 *info)
{
	struct axn500_exercise *exercise;
	int ex;
	char *ptr;

	dprintf("Parsing data for %i exercises, %i bytes\n", num_ex, bytes);
	info->exercises.num = num_ex;
	info->exercises.exercise = malloc(sizeof(struct axn500_exercise) * num_ex);
	if (info->exercises.exercise == NULL) {
		fprintf(stderr, "Not enough memory\n");
		return 1;
	}

	ptr = &data[5];

	/* now process each exercise */
	for (ex = 0; ex < num_ex; ex++) {
		int j;

		dprintf("====================================\n");
		dprintf("Processing exercise %i of %i\n", ex + 1, num_ex);
		exercise = &info->exercises.exercise[ex];

		#if 0
		{
		int i;
		for (i = 0; i < 150; i++) {
			unsigned char c = ptr[i];
			if (i == 95)
				printf("[%02hhx]", c);
			else
				printf(" %02hhx", c);
			if (!((i + 1) % 8))
				printf("\n");
		}
		printf("\n");
		}
		#endif

		exercise->date.day = ptr[EX_DAY_OFFSET];
		/* FIXME - no other info other than day */

		exercise->start_time.second = axn500_parse_hex(ptr[EX_START_TIME_OFFSET]);
		exercise->start_time.minute = axn500_parse_hex(ptr[EX_START_TIME_OFFSET + 1]);
		exercise->start_time.hour = axn500_parse_hex(ptr[EX_START_TIME_OFFSET + 2]);
		if (exercise->start_time.hour > 23 ||
		    exercise->start_time.minute > 59 ||
		    exercise->start_time.second > 59) {
			fprintf(stderr, "Error parsing exercise, invalid start "
				"time (%i:%i:%i)\n",
				exercise->start_time.hour,
				exercise->start_time.minute,
				exercise->start_time.second);
			if (axn500_debug)
				dump_context(ptr, EX_START_TIME_OFFSET, 5);
			return 1;
		}
		dprintf("Got start time %i:%i:%i\n", exercise->start_time.hour,
			exercise->start_time.minute, exercise->start_time.second);

		exercise->duration.second = axn500_parse_hex(ptr[EX_DURATION_OFFSET]);
		exercise->duration.minute = axn500_parse_hex(ptr[EX_DURATION_OFFSET + 1]);
		exercise->duration.hour = axn500_parse_hex(ptr[EX_DURATION_OFFSET + 2]);
		if (exercise->duration.hour > 23 ||
		    exercise->duration.minute > 59 ||
		    exercise->duration.second > 59) {
			fprintf(stderr, "Error parsing exercise, invalid duration "
				"time (%i:%i:%i)\n",
				exercise->duration.hour,
				exercise->duration.minute,
				exercise->duration.second);
			if (axn500_debug)
				dump_context(ptr, EX_DURATION_OFFSET, 5);
			return 1;
		}
		dprintf("Got duration time %i:%i:%i\n", exercise->duration.hour,
			exercise->duration.minute, exercise->duration.second);

		for (j = 0; j < 3; j++) {
			exercise->limits[j].lower = ptr[EX_LIMITS_OFFSET] + (j * 2);
			exercise->limits[j].upper = ptr[EX_LIMITS_OFFSET] + (j * 2) + 1;
		}

		exercise->num_markers = ptr[EX_MARKERNUM_OFFSET];
		exercise->max_hr = ptr[EX_MAX_HR_OFFSET];
		exercise->avg_hr = ptr[EX_AVG_HR_OFFSET];
		exercise->min_alt = (((unsigned char)ptr[EX_MIN_ALT_OFFSET + 1] << 8) +
					(unsigned char)ptr[EX_MIN_ALT_OFFSET]) - 0x300;

		exercise->max_alt = ((ptr[EX_MAX_ALT_OFFSET + 1] << 8) +
					(unsigned char)ptr[EX_MAX_ALT_OFFSET]) - 0x300;
		exercise->kcal = (ptr[EX_KCAL_OFFSET + 1] << 8) + (unsigned char)ptr[EX_KCAL_OFFSET];

		j = (exercise->duration.hour * 60 * 60) +
		    (exercise->duration.minute * 60) +
		    exercise->duration.second;
		/* FIXME - we have fixed 5s periods. need to fetch this from the exercise */
		exercise->entries = j / 5 + ((j % 5)? 1:0);
		exercise->data = malloc(sizeof(struct axn500_entry) * exercise->entries);
		if (exercise->data == NULL) {
			fprintf(stderr, "No enough memory\n");
			for ( ; ex >= 0; ex--) {
				exercise = &info->exercises.exercise[ex];
				free(exercise->data);
			}
			free(info->exercises.exercise);
			return 1;
		}

		/* now find where the data start */
		ptr += EX_HEADER_SIZE +
			(exercise->num_markers - 1) * EX_MARKER_SIZE;

#if 0
		dprintf("guessing the exercise data begins at %li, first "
			"entries:\n", (ptr-data));
		{
		int i;
		unsigned short *s;
		unsigned char hr;
		for (i = 0; i < 10 && i < exercise->entries; i++) {
			hr = ptr[i * 3];
			s = (unsigned short *)&ptr[(i * 3) + 1];
			printf("HR: %hhu %#hhx\talt: %hu %#hx\n", hr,
					ptr[i * 3], *s - 0x300, *s);
		}
		}
#endif

		/* get all the entries */
		for (j = 0; j < exercise->entries; j++) {
			if ((ptr - data) > bytes) {
				fprintf(stderr, "Expected more %i entries "
					"but no enough data",
					exercise->entries - j);
				break;
			}
			exercise->data[j].hr = ptr[0];
			/* the altitude is stored as little endian short, 0x300 is 0 */
			exercise->data[j].altitude = ((ptr[2] << 8) +
				(unsigned char)ptr[1]) - 0x300;
			ptr += 3;
		}
	}
	return 0;
}

/*
 *
 * Every 163 bytes packet begins with a 3 byte header:
 * 0b 00 53
 *       ^^ packet number
 * The packet number decrements until 01, when the command to send more data
 * (0x16, 0x2f) should not be used anymore
 * The data comes in an inverted fashion: first the last exercise header then
 * the last entry of the last exercise, then the last but one exercise header
 * and the last entry of the last but one exercise and so on.
 */
#define AXN500_EX_PKT_SIZE 163
#define AXN500_EX_PKT_HDR_SIZE 3
#define AXN500_EX_PKT_HDR_NUM 2
#define AXN500_EX_PKT_PAYLOAD_SIZE (AXN500_EX_PKT_SIZE - AXN500_EX_PKT_HDR_SIZE)
static char *axn500_get_exercise(int fd, unsigned char *num_ex, int *bytes, struct axn500 *info)
{
	char buff[AXN500_EX_PKT_SIZE], *all;
	int i, rc;
	unsigned char packet_count;
	const char get_exercise_cmd[] = { 0x0b };
	const char get_next_cmd[] = { 0x16, 0x2f };
	const char get_exercisenum_cmd[] = { 0x15 };

	rc = write(fd, get_exercisenum_cmd, 1);
	if (rc < 0) {
		perror("Error writing get exercise count");
		return NULL;
	}
	rc = read(fd, buff, sizeof(buff));
	if (rc < 0) {
		perror("Error getting exercise number");
		return NULL;
	}
	if (rc != 7) {
		fprintf(stderr, "Unexpected reply size while getting number "
			"of exercises (expected 7, got %i)\n", rc);
		return NULL;
	}
	*num_ex = buff[3];

	if (*num_ex == 0) {
		printf("No exercises found on the watch\n");
		return NULL;
	}

	rc = write(fd, get_exercise_cmd, 1);
	if (rc < 0) {
		perror("Error writing get exercises data");
		return NULL;
	}
	/*
	 * we get the first package to have an idea of how much we'll need for
	 * the full thing
	 */
	rc = read(fd, buff, sizeof(buff));
	if (rc < 0) {
		perror("Error getting exercise data");
		return NULL;
	}
	if (rc < 11) {
		fprintf(stderr, "Not enough data, got only %i bytes\n", rc);
		return NULL;
	}

	packet_count = buff[2];
	dprintf("Got %i bytes on the first request for info, total packets: "
		"%i\n", rc, packet_count);

	printf(".");
	fflush(stdout);

	all = malloc(AXN500_EX_PKT_SIZE * packet_count);
	if (all == NULL) {
		fprintf(stderr, "Not enough memory\n");
		return NULL;
	}
	memset(all, 0, AXN500_EX_PKT_SIZE * packet_count);

	memcpy(all, buff, rc);
	*bytes = rc;

	for (i = 1; i < packet_count; i++) {
		unsigned char c;

		rc = write(fd, get_next_cmd, 2);
		if (rc < 0) {
			perror("Error asking for more data");
			free(all);
			return NULL;
		}
		rc = read(fd, buff, sizeof(buff));
		if (rc < 0) {
			perror("Error getting more data");
			free(all);
			return NULL;
		}
		printf(".");
		fflush(stdout);
		if (rc < AXN500_EX_PKT_SIZE && (i + 1) == packet_count)
			fprintf(stderr, "Got less data than expected: %i of "
				"%i, packet %i of %i\n", rc,
				AXN500_EX_PKT_SIZE,
				i + 1, packet_count);
		c = buff[AXN500_EX_PKT_HDR_NUM];
		if (c != (packet_count - i))
			fprintf(stderr, "Unexpected packet number: %i, "
				"expected %i\n", buff[AXN500_EX_PKT_HDR_NUM],
				(packet_count - i));
#if 0
		{
			int c;
			for (c = 0; c < rc; c++) {
				fprintf(stderr, "%hhx\t", buff[c]);
				if (!(c % 8) && c != 0)
					fprintf(stderr, "\n");
			}
			fprintf(stderr, "\n");
		}
#endif
		/* copy data to the buffer skipping the header present in each
		 * packet */
		memcpy(all + *bytes, &buff[AXN500_EX_PKT_HDR_SIZE],
			AXN500_EX_PKT_PAYLOAD_SIZE);
		*bytes += AXN500_EX_PKT_PAYLOAD_SIZE;
	}
	printf("\n");
	dprintf("Receive complete, got %i packets\n", packet_count);

	return all;
}

static int axn500_get_data(int fd, int cmd, struct axn500 *info)
{
	int i, rc;
	char buff[100];

	dprintf("size: %i, [%#x][%#x]\n", axn500_commands[cmd].cmdsize, axn500_commands[cmd].cmd[0],
		axn500_commands[cmd].cmd[1]);
	rc = write(fd, axn500_commands[cmd].cmd, axn500_commands[cmd].cmdsize);
	if (rc < 0) {
		perror("Error writing command");
		return 1;
	}
	if (rc != axn500_commands[cmd].cmdsize) {
		fprintf(stderr, "Short write on command %i\n", cmd);
		return 1;
	}
	dprintf("Wrote cmd %i, waiting for answer...\n", cmd);
	fflush(stdout);

	rc = read(fd, buff, sizeof(buff));
	if (rc < 0) {
		perror("Error reading answer");
		return 1;
	}
	if (axn500_commands[cmd].datasize &&
	    rc != axn500_commands[cmd].datasize) {
		fprintf(stderr, "Incorrect answer size: %i (expected %i) for cmd %i\n",
			rc, axn500_commands[cmd].datasize, cmd);
		return 1;
	}

	dprintf("got %i bytes\n", rc);
	{
		char foo;
		for (i = 0; i < rc; i++) {
			foo = buff[i];
			dprintf("%02x ", (unsigned char)foo);
		}
		dprintf("\n");
	}

	if (axn500_commands[cmd].parser(cmd, info, buff)) {
		fprintf(stderr, "Error parsing reply to command %i\n", cmd);
		return 1;
	}

	return 0;
}

static int axn500_set_data(int fd, int cmd, struct axn500 *info)
{
	int rc, size, cmdsize;
	char raw[100];

	if (axn500_commands[cmd].get_raw == NULL) {
		fprintf(stderr, "BUG: Command %i doesn't have a get_raw method\n", cmd);
		return 1;
	}

	cmdsize = axn500_commands[cmd].cmdsize;

	size = axn500_commands[cmd].get_raw(cmd, info, raw + cmdsize, sizeof(raw) - cmdsize);
	if (size < 0) {
		dprintf("Error getting the raw data for command %i\n", cmd);
		return 1;
	}

	memcpy(raw, axn500_commands[cmd].cmd, cmdsize);

	rc = write(fd, raw, cmdsize + size);
	if (rc < 0) {
		perror("Error while writting command");
		return 1;
	}

	/* now wait for the answer */	
	rc = read(fd, raw, sizeof(raw));
	if (rc < 0) {
		perror("Error reading answer");
		return 1;
	}

	if (rc != (cmdsize + 1)) {
		fprintf(stderr, "Unexpected answer size: %i, %i expected\n",
			rc, cmdsize + 1);
		return 1;
	}

	return 0;
}

void axn500_print_alarm(struct axn500 *info, int i)
{
	printf("%s\t%02i:%02i (%s)", info->alarms[i].desc,
		info->alarms[i].time.hour,
		info->alarms[i].time.minute,
		info->alarms[i].enabled? "enabled":"disabled");
}

void axn500_print_reminder(struct axn500 *info, int i)
{
	printf("%s\t%02i:%02i %02i/%02i/%02i (%s)",
	       info->reminders[i].desc,
	       info->reminders[i].time.hour,
	       info->reminders[i].time.minute,
	       info->reminders[i].date.day,
	       info->reminders[i].date.month,
	       info->reminders[i].date.year,
	       info->reminders[i].enabled? "enabled":"disabled");
}

void axn500_print_timezone(struct axn500 *info, int i)
{
	printf("%02i:%02i (%s)",
		info->timezone[i].hour,
		info->timezone[i].minute,
		(info->enabled_timezone == i)? "main":"");
}

void _axn500_print_date(struct axn500_date *date)
{
	printf("%02i/%02i/%02i", date->day, date->month, date->year);
}

void axn500_print_date(struct axn500 *info)
{
	_axn500_print_date(&info->date);
}

void axn500_print_birthday(struct axn500 *info)
{
	_axn500_print_date(&info->settings.bday);
}

void _axn500_print_limit(struct axn500_limit *limit)
{
	printf("%02i/%02i", limit->lower, limit->upper);
}

void axn500_print_activity_level(struct axn500 *info)
{
	switch (info->settings.activity) {
	case AXN500_SETTINGS_ACTIVITY_LOW:
		printf("low");
		break;
	case AXN500_SETTINGS_ACTIVITY_MEDIUM:
		printf("medium");
		break;
	case AXN500_SETTINGS_ACTIVITY_HIGH:
		printf("high");
		break;
	case AXN500_SETTINGS_ACTIVITY_TOP:
		printf("top");
		break;
	}
}

void axn500_print_activity_button_sound(struct axn500 *info)
{
	printf("%s", (info->settings.activity_button_sound)? "on":"off");
}

void axn500_print_intro_animations(struct axn500 *info)
{
	printf("%s", (info->settings.intro_animations)? "on":"off");
}

void axn500_print_countdown(struct axn500 *info)
{
	printf("%02i:%02i:%02i",
		info->settings.countdown.hour,
		info->settings.countdown.minute,
		info->settings.countdown.second);
}

void axn500_print_sex(struct axn500 *info)
{
	printf("%s", (info->settings.sex == AXN500_SETTINGS_SEX_MALE)? "male":"female");
}

void axn500_print_htouch(struct axn500 *info)
{
	switch (info->settings.htouch) {
	case AXN500_SETTINGS_HTOUCH_OFF:
		printf("off");
		break;
	case AXN500_SETTINGS_HTOUCH_LIGHT:
		printf("light");
		break;
	case AXN500_SETTINGS_HTOUCH_SWITCH_DISPLAY:
		printf("switch display");
		break;
	case AXN500_SETTINGS_HTOUCH_TAKE_LAP:
		printf("take lap");
		break;
	}
}

static void axn500_print_info(struct axn500 *info)
{
	int i;

	printf("Date: (dd/mm/yy)\n\t");
	axn500_print_date(info);
	printf("\n");

	printf("Clock:\n");
	for (i = 0; i < 2; i++) {
		printf("\tTime %i", i);
		axn500_print_timezone(info, i);
		printf("\n");
	}
	printf("Alarms:\n");
	for (i = 0; i < 3; i++) {
		printf("\t");
		axn500_print_alarm(info, i);
		printf("\n");
	}

	printf("Reminders:\n");
	for (i = 0; i < 5; i++) {
		printf("\t");
		axn500_print_reminder(info, i);
		printf("\n");
	}

	printf("Settings:\n");
	printf("\tBirthday: ");
	axn500_print_birthday(info);
	printf("\n");
	printf("\tHeight: %icm\n", info->settings.height);
	printf("\tWeight: %ilb\n", info->settings.weight);
	printf("\tRecord Rate: %is\n", info->settings.record_rate);
	printf("\tActivity level: ");
	axn500_print_activity_level(info);
	printf("\n");
	printf("\tHR max: %i\n", info->settings.hrmax);
	printf("\tVOmax: %i\n", info->settings.vomax);
	printf("\tSit HR: %i\n", info->settings.sit_hr);
	printf("\tActivity button sound: ");
	axn500_print_activity_button_sound(info);
	printf("\n");
	printf("\tIntro animations: ");
	axn500_print_intro_animations(info);
	printf("\n");
	printf("\tUnits: %s\n", (info->settings.imperial)? "imperial":"metric");
	printf("\tDeclination: %i\n", info->settings.declination);
	printf("\tCountdown (hh:mm:ss): ");
	axn500_print_countdown(info);
	printf("\n");
	printf("\tSex: ");
	axn500_print_sex(info);
	printf("\n");
	printf("\tHeart touch: ");
	axn500_print_htouch(info);
	printf("\n");
}

int axn500_init(void)
{
	int fd;

	fd = socket(AF_IRDA, SOCK_STREAM, 0);
	if (fd < 0)
		perror("Unable to create socket");

	return fd;
}

int axn500_connect(int fd, int wait)
{
	struct sockaddr_irda addr;

	do {
		if (irda_discover_devices(fd, &addr, 10)) {
			if (errno == EAGAIN) {
				/* keep trying */
				sleep(1);
				continue;
			}
			perror("Error scanning for devices");
			return 1;
		} else
			break;
	} while(wait);

	addr.sir_family = AF_IRDA;
	strncpy(addr.sir_name, "HRM", sizeof(addr.sir_name));
	if (connect(fd, (struct sockaddr *)&addr, sizeof(addr))) {
		perror("Error connecting");
		return 1;
	}
	dprintf("connected\n");

	return 0;
}

void axn500_set_debug(int debug)
{
	axn500_debug = debug;
}

int axn500_fetch_all(int fd, struct axn500 *info)
{
	int i, rc;
	int cmds[] = { AXN500_CMD_GET_TIME,
		       AXN500_CMD_GET_REMINDER1,
		       AXN500_CMD_GET_REMINDER2,
		       AXN500_CMD_GET_REMINDER3,
		       AXN500_CMD_GET_REMINDER4,
		       AXN500_CMD_GET_REMINDER5,
		       AXN500_CMD_GET_SETTINGS,
		       -1 };

	for (i = 0; cmds[i] != -1; i++) {
		rc = axn500_get_data(fd, i, info);
		if (rc)
			return rc;
	}

	return 0;
}

/* client application */
static int show_all(int wait)
{
	int rc, fd = axn500_init();
	struct axn500 info;

	if (fd < 0)
		return fd;

	rc = axn500_connect(fd, wait);
	if (rc)
		return rc;

	rc = axn500_fetch_all(fd, &info);
	if (rc)
		return rc;

	axn500_print_info(&info);
	return 0;
}

static int get_value(char *value, int wait)
{
	int rc, fd = axn500_init(), i, done_data = 0, multi = 0;
	struct axn500 info;
	char *ptr, *start = value, *saved;
	struct {
		char *name;
		int cmd;
	} values[] = {
		/* FIXME: be more specific, e.g. alarm1.date */
		{ "alarm1", AXN500_CMD_GET_TIME },
		{ "alarm2", AXN500_CMD_GET_TIME },
		{ "alarm3", AXN500_CMD_GET_TIME },
		{ "reminder1", AXN500_CMD_GET_REMINDER1 },
		{ "reminder2", AXN500_CMD_GET_REMINDER2 },
		{ "reminder3", AXN500_CMD_GET_REMINDER3 },
		{ "reminder4", AXN500_CMD_GET_REMINDER4 },
		{ "reminder5", AXN500_CMD_GET_REMINDER5 },
		{ "timezone1", AXN500_CMD_GET_TIME },
		{ "timezone2", AXN500_CMD_GET_TIME },
		{ "timezone", AXN500_CMD_GET_TIME },
		{ "ampm", AXN500_CMD_GET_TIME },
		{ "date", AXN500_CMD_GET_TIME },
		{ "birthday", AXN500_CMD_GET_SETTINGS },
		{ "height", AXN500_CMD_GET_SETTINGS },
		{ "weight", AXN500_CMD_GET_SETTINGS },
		{ "record_rate", AXN500_CMD_GET_SETTINGS },
		{ "activity", AXN500_CMD_GET_SETTINGS },
		{ "hrmax", AXN500_CMD_GET_SETTINGS },
		{ "vomax", AXN500_CMD_GET_SETTINGS },
		{ "sit_hr", AXN500_CMD_GET_SETTINGS },
		{ "activity_button_sound", AXN500_CMD_GET_SETTINGS },
		{ "intro_animations", AXN500_CMD_GET_SETTINGS },
		{ "imperial", AXN500_CMD_GET_SETTINGS },
		{ "declination", AXN500_CMD_GET_SETTINGS },
		{ "countdown", AXN500_CMD_GET_SETTINGS },
		{ "sex", AXN500_CMD_GET_SETTINGS },
		{ "htouch", AXN500_CMD_GET_SETTINGS },
		{ NULL, 0 },
	};

	if (!strcmp(value, "help")) {
		for (i = 0; values[i].name != NULL; i++)
			printf("%s\n", values[i].name);
		return 0;
	}

	if (fd < 0)
		return fd;

	rc = axn500_connect(fd, wait);
	if (rc)
		return rc;

	if (strchr(value, ',')) {
		/* multiple values, fetch all the data */
		rc = axn500_fetch_all(fd, &info);
		if (rc)
			return rc;
		done_data = 1;
	}

	while (1) {
		ptr = strtok_r(start, ",", &saved);
		if (ptr == NULL)
			break;
		start = NULL;

		for (i = 0; values[i].name != NULL && strcmp(values[i].name, ptr); i++)
			;

		if (values[i].name == NULL) {
			fprintf(stderr, "value %s unsupported\n", ptr);
			return -1;
		}

		if (done_data == 0) {
			rc = axn500_get_data(fd, values[i].cmd, &info);
			if (rc)
				return -1;
		} 

		if (multi)
			printf(";");

		if (!strcmp(values[i].name, "alarm1")) {
			axn500_print_alarm(&info, 0);
		} else if (!strcmp(values[i].name, "alarm2")) {
			axn500_print_alarm(&info, 1);
		} else if (!strcmp(values[i].name, "alarm3")) {
			axn500_print_alarm(&info, 2);
		} else if (!strcmp(values[i].name, "reminder1")) {
			axn500_print_reminder(&info, 0);
		} else if (!strcmp(values[i].name, "reminder2")) {
			axn500_print_reminder(&info, 1);
		} else if (!strcmp(values[i].name, "reminder3")) {
			axn500_print_reminder(&info, 2);
		} else if (!strcmp(values[i].name, "reminder4")) {
			axn500_print_reminder(&info, 3);
		} else if (!strcmp(values[i].name, "reminder5")) {
			axn500_print_reminder(&info, 4);
		} else if (!strcmp(values[i].name, "timezone1")) {
			axn500_print_timezone(&info, 0);
		} else if (!strcmp(values[i].name, "timezone2")) {
			axn500_print_timezone(&info, 1);
		} else if (!strcmp(values[i].name, "timezone")) {
			printf("%i", info.enabled_timezone + 1);
		} else if (!strcmp(values[i].name, "ampm")) {
			printf("%i", info.ampm);
		} else if (!strcmp(values[i].name, "date")) {
			axn500_print_date(&info);
		} else if (!strcmp(values[i].name, "birthday")) {
			axn500_print_birthday(&info);
		} else if (!strcmp(values[i].name, "height")) {
			printf("%i", info.settings.height);
		} else if (!strcmp(values[i].name, "weight")) {
			printf("%i", info.settings.weight);
		} else if (!strcmp(values[i].name, "record_rate")) {
			printf("%i", info.settings.record_rate);
		} else if (!strcmp(values[i].name, "activity")) {
			axn500_print_activity_level(&info);
		} else if (!strcmp(values[i].name, "hrmax")) {
			printf("%i", info.settings.hrmax);
		} else if (!strcmp(values[i].name, "sit_hr")) {
			printf("%i", info.settings.sit_hr);
		} else if (!strcmp(values[i].name, "activity_button_sound")) {
			axn500_print_activity_button_sound(&info);
		} else if (!strcmp(values[i].name, "intro_animations")) {
			axn500_print_intro_animations(&info);
		} else if (!strcmp(values[i].name, "imperial")) {
			printf("%s", info.settings.imperial? "on":"off");
		} else if (!strcmp(values[i].name, "declination")) {
			printf("%i", info.settings.declination);
		} else if (!strcmp(values[i].name, "countdown")) {
			axn500_print_countdown(&info);
		} else if (!strcmp(values[i].name, "sex")) {
			axn500_print_sex(&info);
		} else if (!strcmp(values[i].name, "htouch")) {
			axn500_print_htouch(&info);
		}
		/* value */
		multi = 1;
	}
	printf("\n");

	return 0;
}
	
static void print_exercises(struct axn500 *info, FILE *output)
{
	int i, j;

	for (i = 0; i < info->exercises.num; i++) {
		struct axn500_exercise *e = &info->exercises.exercise[i];

		fprintf(output, "Exercise %i\n", i);
		fprintf(output, "Date: ");
		_axn500_print_date(&e->date);
		fprintf(output, "\n");
		fprintf(output, "Start time: %i:%i:%i", e->start_time.hour,
			e->start_time.minute, e->start_time.second);
		fprintf(output, "\n");
		fprintf(output, "Duration: %ih%imin%is", e->duration.hour,
			e->duration.minute, e->duration.second);
		fprintf(output, "\n");
		for (j = 0; j < 3; j++) {
			fprintf(output, "Limit %i (lower/upper): ", j);
			_axn500_print_limit(&e->limits[j]);
			fprintf(output, "\n");
		}
		fprintf(output, "KCal: %hi\n", e->kcal);
		fprintf(output, "Markers: %i\n", e->num_markers);
		fprintf(output, "Maximum HR: %i\n", e->max_hr);
		fprintf(output, "Average HR: %i\n", e->avg_hr);
		fprintf(output, "Minimum altitude: %i\n", e->min_alt);
		fprintf(output, "Maximum altitude: %i\n", e->max_alt);
		fprintf(output, "Data:\n");
		for (j = 0; j < e->entries; j++)
			fprintf(output, "%i\t%i\n", e->data[j].hr, e->data[j].altitude);
	}
}

static int get_all_exercises(FILE *output, int wait, const char *save)
{
	int rc, fd = axn500_init(), bytes;
	struct axn500 info;
	char *ex;
	unsigned char num_ex;

	if (fd < 0) {
		fprintf(stderr, "Unable to initialize AXN500\n");
		return 1;
	}
	rc = axn500_connect(fd, wait);
	if (rc)
		return rc;

	ex = axn500_get_exercise(fd, &num_ex, &bytes, &info);
	if (ex == NULL) {
		fprintf(stderr, "Unable to get exercises from AXN500\n");
		return 1;
	}

	printf("Found %i exercises\n", num_ex);
	if (num_ex == 0)
		return 0;

	if (save) {
		int fd = open(save, O_CREAT | O_TRUNC | O_RDWR,
				S_IRUSR | S_IWUSR);
		if (fd < 0) {
			perror("Error creating file");
			return 1;
		}
		write(fd, &num_ex, 1);
		/* FIXME - not endian safe */
		write(fd, &bytes, 4);
		dprintf("Writing %i bytes\n", bytes);
		write(fd, ex, bytes);
		close(fd);
		return 0;
	}

	rc = axn500_parse_exercises(ex, num_ex, bytes, &info);
	if (rc) {
		fprintf(stderr, "Unable to parse exercise data from AXN500\n");
		return 1;
	}
	free(ex);

	print_exercises(&info, output);

	return 0;
}

static int parse_exercises(const char *filename, FILE *output)
{
	int fd = open(filename, O_RDONLY), rc;
	struct axn500 info;
	char *ex;
	unsigned char num_ex;
	unsigned int bytes;

	if (fd < 0) {
		perror("Unable to open file");
		return 1;
	}

	if (read(fd, &num_ex, 1) != 1) {
		perror("Unable to read number of exercises");
		close(fd);
		return 1;
	}
	if (read(fd, &bytes, sizeof(bytes)) != sizeof(bytes)) {
		perror("Unable to read the exercise size");
		close(fd);
		return 1;
	}
	if (num_ex == 0 || num_ex > 50) {
		fprintf(stderr, "Invalid number of exercises (%i). Corrupt file?\n", num_ex);
		close(fd);
		return 1;
	}
	if (bytes <= 3 || bytes > 50000) {
		fprintf(stderr, "Invalid exercise size (%i). Corrupt file?\n", bytes);
		close(fd);
		return 1;
	}
	ex = malloc(bytes);
	if (ex == NULL) {
		perror("Unable to allocate memory");
		close(fd);
		return 1;
	}
	rc = read(fd, ex, bytes);
	if (rc != bytes) {
		if (rc < 0) {
			perror("Unable to read data file");
			free(ex);
			close(fd);
			return 1;
		} else {
			fprintf(stderr, "Short read while reading data file: "
				"wanted %i, got %i\n", bytes, rc);
			free(ex);
			close(fd);
			return 1;
		}
	}
	close(fd);

	rc = axn500_parse_exercises(ex, num_ex, bytes, &info);
	if (rc) {
		fprintf(stderr, "Unable to parse exercise data from AXN500\n");
		free(ex);
		return 1;
	}
	free(ex);

	print_exercises(&info, output);

	return 0;
}

static void show_help(FILE *output)
{
	fprintf(output, "axn500 version %s\n\n", version);

	fprintf(output, "axn500 [options] <command>\n");
	fprintf(output, "Commands:\n");
	fprintf(output, "\t-a\t\tprint all available settings\n");
	fprintf(output, "\t-g <value>\tget a value from the watch. use 'help' for the list\n");
	fprintf(output, "\t\t\tmultiple values can be get at once using comma separated list\n");
	fprintf(output, "\t-e\t\tget all exercises\n");

	fprintf(output, "\n\t-s <file>\tget all exercises and save in the specified file\n");
	fprintf(output, "\t-p <file>\tparse a raw exercises file and print the result\n");

	fprintf(output, "\nOptions:\n");
	fprintf(output, "\t-d\t\tenable debug\n");
	fprintf(output, "\t-n\t\tdon't wait for the watch to be in range\n");

	fprintf(output, "\n\t-h\t\tprint this message\n");
}

static char *options = "andeg:p:s:h";
int main(int argc, char *argv[])
{
	int opt, wait = 1;

	while ((opt = getopt(argc, argv, options)) != -1) {
		switch(opt) {
			case 'a':
				return show_all(wait);
			case 'g':
				return get_value(optarg, wait);
			case 'd':
				axn500_set_debug(1);
				break;
			case 'n':
				wait = 0;
				break;
			case 'e':
				return get_all_exercises(stdout, wait, NULL);
			case 's':
				return get_all_exercises(stdout, wait, optarg);
			case 'p':
				return parse_exercises(optarg, stdout);
			case 'h':
				show_help(stdout);
				exit(0);
			default:
				fprintf(stderr, "Unknown option: %c\n", opt);
				show_help(stderr);
				exit(1);
		}
	}
	show_help(stdout);

	return 0;
}

