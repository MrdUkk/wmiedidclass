/*
 This is a port from READ-EDID project by Matthew Kern (poly-p man) <pyrophobicman@gmail.com>
 WEB: http://www.polypux.org/projects/read-edid/

 some minor modifications made by dUkk to work with WMI wrapper and add serial number+mon size infos
 so credits should go to 2 people
 
 (c) 2013-2014
*/

#include "edid.h"
#include <windows.h>
#include <stdio.h>


bool parse_edid(byte* edid, edid_info *ptr_to_edid)
{
	unsigned i;
	byte* block;
	char* monitor_name = NULL;
	char *monitor_serial = NULL;
	char monitor_alt_name[100];
	byte checksum = 0;
	char *vendor_sign;
	bool ret=true;
	int production_week, production_year, model_year;
	bool is_model_year = false;
	double width_mm, height_mm, aspect_ratio=0;

	for( i = 0; i < EDID_LENGTH; i++ )
		checksum += edid[ i ];

	if (  checksum != 0  ) {
		//fprintf(stderr, "EDID checksum failed - data is corrupt. Continuing anyway." );
		ret = false;
	} 
	//else 
	//	fprintf(stderr, "EDID checksum passed." );


	if ( strncmp( (const char *)edid+EDID_HEADER, (const char *)edid_v1_header, EDID_HEADER_END+1 ) )
	{
		//fprintf(stderr, "first bytes don't match EDID version 1 header" );
		//fprintf(stderr, "do not trust output (if any)." );
		ret = false;
	}

	//printf( "\n\t# EDID version %d revision %d\n", (int)edid[EDID_STRUCT_VERSION],(int)edid[EDID_STRUCT_REVISION] );

	vendor_sign = get_vendor_sign( edid + ID_MANUFACTURER_NAME ); 

	//printf( "Section \"Monitor\"\n" );

	switch(edid[MANUFACTURE_WEEK])
	{
	case 0x00:
		production_week = -1;
		break;
	case 0xff:
		production_week = -1;
		is_model_year = true;
		break;
	default:
		production_week = edid[MANUFACTURE_WEEK];
		break;
	}
	if (is_model_year)
	{
		production_year = -1;
		model_year = 1990 + edid[MANUFACTURE_YEAR];
	}
	else
	{
		production_year = 1990 + edid[MANUFACTURE_YEAR];
		model_year = -1;
	}

	//monitor size

	/* Screen Size / Aspect Ratio */
	if (edid[MONITOR_HOR_SIZE] == 0 && edid[MONITOR_VER_SIZE] == 0)
	{
		width_mm = -1;
		height_mm = -1;
		aspect_ratio = -1.0;
	}
	else if (edid[MONITOR_VER_SIZE] == 0)
	{
		width_mm = -1;
		height_mm = -1; 
		aspect_ratio = 100.0 / (edid[MONITOR_HOR_SIZE] + 99);
	}
	else if (edid[MONITOR_HOR_SIZE] == 0)
	{
		width_mm = -1;
		height_mm = -1;
		aspect_ratio = 100.0 / (edid[MONITOR_VER_SIZE] + 99);
		aspect_ratio = 1/aspect_ratio; /* portrait */
	}
	else
	{
		width_mm = 10* edid[MONITOR_HOR_SIZE];
		height_mm = 10* edid[MONITOR_VER_SIZE];
	}
	ptr_to_edid->monsizeh=(int)width_mm;
	ptr_to_edid->monsizev=(int)height_mm;
	//printf("\tMonitor dimension: %.1f x %.1f mm, aspect=%.1f\n", width_mm, height_mm, aspect_ratio);

	ptr_to_edid->mfgyear=production_year;
	ptr_to_edid->mfgweek=production_week;
	ptr_to_edid->mfgmodelyear=model_year;
	//printf( "\tproduction year=%d, week=%d, model year=%d\n", production_year, production_week, model_year);


	block = edid + DETAILED_TIMING_DESCRIPTIONS_START;

	for( i = 0; i < NO_DETAILED_TIMING_DESCRIPTIONS; i++,
		block += DETAILED_TIMING_DESCRIPTION_SIZE )
	{

		if ( block_type( block ) == MONITOR_NAME )
		{
			monitor_name = get_monitor_name( block );
			break;
		}
	}

	if (!monitor_name) {
		/* Stupid djgpp hasn't snprintf so we have to hack something together */
		if(strlen(vendor_sign) + 10 > sizeof(monitor_alt_name))
			vendor_sign[3] = 0;

		sprintf(monitor_alt_name, "%s:%02x%02x",
			vendor_sign, edid[ID_MODEL], edid[ID_MODEL+1]) ;
		monitor_name = monitor_alt_name;
	}


	strcpy(ptr_to_edid->identifier, monitor_name);
	strcpy(ptr_to_edid->vendor, vendor_sign);
	strcpy(ptr_to_edid->modelname, monitor_name);
	//printf( "\tIdentifier \"%s\"\n", monitor_name );
	//printf( "\tVendorName \"%s\"\n", vendor_sign );
	//printf( "\tModelName \"%s\"\n", monitor_name );

	block = edid + DETAILED_TIMING_DESCRIPTIONS_START;

	for( i = 0; i < NO_DETAILED_TIMING_DESCRIPTIONS; i++,
		block += DETAILED_TIMING_DESCRIPTION_SIZE )
	{

		if ( block_type( block ) == MONITOR_LIMITS )
			parse_monitor_limits( block );
	}

	parse_dpms_capabilities(edid[DPMS_FLAGS]);

	block = edid + DETAILED_TIMING_DESCRIPTIONS_START;

	for( i = 0; i < NO_DETAILED_TIMING_DESCRIPTIONS; i++,
		block += DETAILED_TIMING_DESCRIPTION_SIZE )
	{
		switch(block_type( block ))
		{
		case DETAILED_TIMING_BLOCK:
			{
				parse_timing_description( block, ptr_to_edid );
				break;
			}
		case MONITOR_SERIAL:
			{
				monitor_serial = get_monitor_name( block );
				if(strlen(monitor_serial) > 0)
					strcpy(ptr_to_edid->serial, monitor_serial);
				else
					strcpy(ptr_to_edid->serial, "-");
				//printf("Serial: %s\n", monitor_serial);
				break;
			}
		}
	}

	//printf( "EndSection\n" );

	return ret;
}


int
parse_timing_description( byte* dtd, edid_info *ptr_to_edid )
{
  int htotal, vtotal;
  htotal = H_ACTIVE + H_BLANKING;
  vtotal = V_ACTIVE + V_BLANKING;
  
  sprintf(ptr_to_edid->mode, "%dx%d", H_ACTIVE, V_ACTIVE);
  //printf( "\tMode \t\"%dx%d\"", H_ACTIVE, V_ACTIVE );
  /*
  printf( "\t# vfreq %3.3fHz, hfreq %6.3fkHz\n",
          (double)PIXEL_CLOCK/((double)vtotal*(double)htotal),
          (double)PIXEL_CLOCK/(double)(htotal*1000));

  printf( "\t\tDotClock\t%f\n", (double)PIXEL_CLOCK/1000000.0 );

  printf( "\t\tHTimings\t%u %u %u %u\n", H_ACTIVE,
          H_ACTIVE+H_SYNC_OFFSET, 
          H_ACTIVE+H_SYNC_OFFSET+H_SYNC_WIDTH,
          htotal );

  printf( "\t\tVTimings\t%u %u %u %u\n", V_ACTIVE,
          V_ACTIVE+V_SYNC_OFFSET,
          V_ACTIVE+V_SYNC_OFFSET+V_SYNC_WIDTH,
          vtotal );
*/
  ptr_to_edid->dispsizeh=H_SIZE;
  ptr_to_edid->dispsizev=V_SIZE;
  //printf( "\tDisplay size\n\t\tHorizontal %u mm\n\t\tVertical   %u mm\n", H_SIZE, V_SIZE);

  if ( INTERLACED || (SYNC_TYPE == SYNC_SEPARATE)) {
	  /*
    printf( "\t\tFlags\t%s\"%sHSync\" \"%sVSync\"\n",
            INTERLACED ? "\"Interlace\" ": "",
            HSYNC_POSITIVE ? "+": "-",
            VSYNC_POSITIVE ? "+": "-");
			*/
  }

  //printf( "\tEndMode\n" );

  return 0;
}


int
block_type( byte* block )
{
  if ( !strncmp((const char *)edid_v1_descriptor_flag, (const char *)block, 2 ) )
    {
      //printf("\t# Block type: 2:%x 3:%x\n", block[2], block[3]);

      /* descriptor */

      if ( block[ 2 ] != 0 )
        return UNKNOWN_DESCRIPTOR;


      return block[ 3 ];
    } else {

    /* detailed timing block */

    return DETAILED_TIMING_BLOCK;
  }
}

char*
get_monitor_name( byte const* block )
{
  static char name[ 13 ];
  unsigned i;
  byte const* ptr = block + DESCRIPTOR_DATA;


  for( i = 0; i < 13; i++, ptr++ )
    {

      if ( *ptr == 0xa )
        {
          name[ i ] = 0;
          return name;
        }

      name[ i ] = *ptr;
    }


  return name;
}


char* get_vendor_sign( byte const* block )
{
  static char sign[4];
  unsigned short h;

  /*
    08h WORD    big-endian manufacturer ID (see #00136)
    bits 14-10: first letter (01h='A', 02h='B', etc.)
    bits 9-5: second letter
    bits 4-0: third letter
  */
  h = COMBINE_HI_8LO(block[0], block[1]);
  sign[0] = ((h>>10) & 0x1f) + 'A' - 1;
  sign[1] = ((h>>5) & 0x1f) + 'A' - 1;
  sign[2] = (h & 0x1f) + 'A' - 1;
  sign[3] = 0;
  return sign;
}

int
parse_monitor_limits( byte* block )
{
	/*
  printf( "\tHorizSync %u-%u\n", H_MIN_RATE, H_MAX_RATE );
  printf( "\tVertRefresh %u-%u\n", V_MIN_RATE, V_MAX_RATE );
  if ( MAX_PIXEL_CLOCK == 10*0xff )
    printf( "\t# Max dot clock not given\n" );
  else
    printf( "\t# Max dot clock (video bandwidth) %u MHz\n", (int)MAX_PIXEL_CLOCK );

  if ( GTF_SUPPORT )
    {
      printf( "\t# EDID version 3 GTF given: contact author\n" );
    }
  */
  return 0;
}

int
parse_dpms_capabilities(byte flags)
{
	/*
  printf("\t# DPMS capabilities: Active off:%s  Suspend:%s  Standby:%s\n\n",
         (flags & DPMS_ACTIVE_OFF) ? "yes" : "no",
         (flags & DPMS_SUSPEND)    ? "yes" : "no",
         (flags & DPMS_STANDBY)    ? "yes" : "no");
		 */
  return 0;
}
