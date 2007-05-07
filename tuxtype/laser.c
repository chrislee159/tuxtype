/***************************************************************************
 -  file: laser.c
 -  description: a modification of TuxMath for typing :)
                            -------------------
    begin                : 
    copyright            : Bill Kendrick (C) 2002
                           Jesse Andrews (C) 2003
    email                : tuxtype-dev@tux4kids.net
***************************************************************************/

/***************************************************************************
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
*                                                                         *
***************************************************************************/

#include "globals.h"
#include "funcs.h"
#include "laser.h"

sprite * shield;
SDL_Surface * images[NUM_IMAGES];
Mix_Chunk * sounds[NUM_SOUNDS];
Mix_Music * musics[NUM_MUSICS];
SDL_Surface * bkgd;

/* --- unload all media --- */
void laser_unload_data(void) {
	int i;

	for (i = 0; i < NUM_IMAGES; i++)
		SDL_FreeSurface(images[i]);

	if (sys_sound) {
		for (i = 0; i < NUM_SOUNDS; i++)
			Mix_FreeChunk(sounds[i]);
		for (i = 0; i < NUM_MUSICS; i++)
			Mix_FreeMusic(musics[i]);
	}

	FreeSprite(shield);

	pause_unload_media();

	for ( i=1; i<255; i++ )
		SDL_FreeSurface( letters[i] );

	TTF_CloseFont(font);
}

/* --- Load all media --- */
void laser_load_data(void) {
	int i;
	font = LoadFont( ttf_font, 32);

	/* Load images: */
	for (i = 0; i < NUM_IMAGES; i++) 
		images[i] = LoadImage(image_filenames[i], IMG_ALPHA);
	shield = LoadSprite( "cities/shield", IMG_ALPHA );

	if (sys_sound) {
		for (i = 0; i < NUM_SOUNDS; i++)
			sounds[i] = LoadSound(sound_filenames[i]);

		for (i = 0; i < NUM_MUSICS; i++)
			musics[i] = LoadMusic(music_filenames[i]);
	}

	pause_load_media();

        /* Now that the words are stored internally as wchars, we use the */
        /* Unicode glyph version of black_outline():                      */
        {
          wchar_t t;
          int i;

          for (i = 1; i < 255; i++)
          {
            t = (wchar_t)i;

            DEBUGCODE
            {
              fprintf(stderr, "Creating SDL_Surface for int = %d, char = %lc\n", i, t);
            }

            letters[i] = black_outline_wchar(t, font, &white);
          }
        }
}


#define FPS (1000 / 15)   /* 15 fps max */
#define CITY_EXPL_START 3 * 5  /* Must be mult. of 5 (number of expl frames) */
#define COMET_EXPL_START 2 * 2 /* Must be mult. of 2 (number of expl frames) */
#define ANIM_FRAME_START 4 * 2 /* Must be mult. of 2 (number of tux frames) */
#define GAMEOVER_COUNTER_START 75
#define LEVEL_START_WAIT_START 20
#define LASER_START 5
#define NUM_ANS 8

/* Local (to game.c) 'globals': */

int wave, speed, score, pre_wave_score, num_attackers, distanceMoved;
unsigned char ans[NUM_ANS];
int ans_num;

comet_type comets[MAX_COMETS];
city_type cities[NUM_CITIES];
laser_type laser;

/* Local function prototypes: */

void laser_reset_level(int DIF_LEVEL);
void laser_add_comet(int DIF_LEVEL);
void laser_draw_numbers(unsigned char * str, int x);
void laser_draw_line(int x1, int y1, int x2, int y2, int r, int g, int b);
void laser_putpixel(SDL_Surface * surface, int x, int y, Uint32 pixel);
void laser_draw_console_image(int i);
void laser_draw_let(unsigned char c, int x, int y);
void laser_add_score(int inc);

/* --- MAIN GAME FUNCTION!!! --- */

int laser_game(int DIF_LEVEL)
{
	int i, img, done, quit, frame, lowest, lowest_y, 
	    tux_img, old_tux_img, tux_pressing, tux_anim, tux_anim_frame,
	    tux_same_counter, level_start_wait, num_cities_alive,
	    num_comets_alive, paused, picked_comet, 
	    gameover;

	SDL_Event event;
	Uint32    last_time, now_time;
	SDLKey    key;
	SDL_Rect  src, dest;
	unsigned char      str[64];

	LOG( "starting Comet Zap game\n" );
	DOUT( DIF_LEVEL );

	SDL_ShowCursor(0);
	laser_load_data();

	/* Clear window: */
  
	SDL_FillRect(screen, NULL, SDL_MapRGB(screen->format, 0, 0, 0));
	SDL_Flip(screen);

	/* --- MAIN GAME LOOP: --- */

	done = 0;
	quit = 0;
  
	/* Prepare to start the game: */
  
	wave = 1;
	score = 0;
	gameover = 0;
	level_start_wait = LEVEL_START_WAIT_START;

	
	/* (Create and position cities) */
  
	for (i = 0; i < NUM_CITIES; i++) {
		cities[i].alive = 1;
		cities[i].expl = 0;
		cities[i].shields = 1;

		if (NUM_CITIES % 2 == 0) {
			/* Left vs. Right - makes room for Tux and the console */

			if (i < NUM_CITIES / 2) 
				cities[i].x = (((screen->w / (NUM_CITIES + 1)) * i) + ((images[IMG_CITY_BLUE] -> w) / 2));
			else
				cities[i].x = (screen->w - ((((screen->w / (NUM_CITIES + 1)) * (i - (NUM_CITIES / 2)) + ((images[IMG_CITY_BLUE] -> w) / 2)))));
		} else {
			/* put them in order across the bottom of     *
			 * the screen so we can do word's in order!!! */
			cities[i].x = i*screen->w / (NUM_CITIES) + images[IMG_CITY_BLUE]->w/2;
		}
	}

	num_cities_alive = NUM_CITIES;
	num_comets_alive = 0;


	/* (Clear laser) */

	laser.alive = 0;

  
	/* Reset remaining stuff: */
 
	bkgd = NULL;
	laser_reset_level(DIF_LEVEL);
  
	/* --- MAIN GAME LOOP!!! --- */
  
	frame = 0;
	paused = 0;
	picked_comet = -1;
	tux_img = IMG_TUX_RELAX1;
	tux_anim = -1;
	tux_anim_frame = 0;
	tux_same_counter = 0;
	ans_num = 0;

	/* Next line changed to get rid of int_rand() which didn't work on win32: */
	audioMusicPlay(musics[MUS_GAME + (rand() % NUM_MUSICS)], 0);

	do {

		frame++;
		last_time = SDL_GetTicks();

		old_tux_img = tux_img;
		tux_pressing = 0;

		/* Handle any incoming events: */
     
		while (SDL_PollEvent(&event) > 0) {

			if (event.type == SDL_QUIT) {
				/* Window close event - quit! */
				exit(0);
	      
			} else if (event.type == SDL_KEYDOWN) {

				key = event.key.keysym.sym;
	      
				if (key == SDLK_F11)
					SDL_SaveBMP( screen, "laser.bmp");

				if (key == SDLK_ESCAPE)
					paused = 1;

				/* --- eat other keys until level wait has passed --- */ 
				if (level_start_wait > 0) 
					key = SDLK_UNKNOWN;
				
				if (((event.key.keysym.unicode & 0xff)>=97) & ((event.key.keysym.unicode & 0xff)<=122)) {
					ans[ans_num++] = KEYMAP[(event.key.keysym.unicode & 0xff)-32];
					tux_pressing ++;
				}else{
					ans[ans_num++] = KEYMAP[event.key.keysym.unicode & 0xff];
					tux_pressing ++;
				}
			}
		}
      
      
		/* Handle answer: */

		for (;ans_num>0;ans_num--) {

			/*  Pick the lowest comet which has the right answer: */
	
			lowest_y = 0;
			lowest = -1;
	
			for (i = 0; i < MAX_COMETS; i++)
				if (comets[i].alive && comets[i].expl == 0 && 
				    KEYMAP[comets[i].ch] == ans[ans_num-1] && comets[i].y > lowest_y) {
					lowest = i;
					lowest_y = comets[i].y;
				}
	
	
			/* If there was an comet with this answer, destroy it! */
	
			if (lowest != -1) {

				/* Destroy comet: */
		  
				comets[lowest].expl = COMET_EXPL_START;
	    
				/* Fire laser: */

				laser.alive = LASER_START;

				/* this is a hack so drawing to the center of the screen works */
				if (abs(comets[lowest].x - screen->w/2) < 10) {
					laser.x1 = screen->w / 2;
					laser.y1 = screen->h;
	    
					laser.x2 = laser.x1;
					laser.y2 = comets[lowest].y;
				} else {
					laser.x1 = screen->w / 2;
					laser.y1 = screen->h;
	    
					laser.x2 = comets[lowest].x;
					laser.y2 = comets[lowest].y;
				}
	    
				playsound(sounds[SND_LASER]);
	    
				/* 50% of the time.. */
	    
				if (0 == (rand() % 2))  {

					/* ... pick an animation to play: */ 
					if (0 == (rand() % 2))
						tux_anim = IMG_TUX_YES1;
					else
						tux_anim = IMG_TUX_YAY1;
	        
					tux_anim_frame = ANIM_FRAME_START;
				}

				/* Increment score: */

				laser_add_score( (DIF_LEVEL+1) * 5 * ((screen->h - comets[lowest].y)/20 ));

			} else {

				/* Didn't hit anything! */
	    
				playsound(sounds[SND_BUZZ]);
	    
				if (0 == (rand() % 2))
					tux_img = IMG_TUX_DRAT;
				else
					tux_img = IMG_TUX_YIPE;

				laser_add_score( -25 * wave);
			}
		}

      
		/* Handle start-wait countdown: */
      
		if (level_start_wait > 0) {

			level_start_wait--;
	  
			if (level_start_wait > LEVEL_START_WAIT_START / 4)
				tux_img = IMG_TUX_RELAX1;
			else if (level_start_wait > 0)
				tux_img = IMG_TUX_RELAX2;
			else
				tux_img = IMG_TUX_SIT;
	  
			if (level_start_wait == LEVEL_START_WAIT_START / 4)
				playsound(sounds[SND_ALARM]);
		}

      
		/* If Tux pressed a button, pick a new (different!) stance: */
	  
		if (tux_pressing) {
			while (tux_img == old_tux_img)
				tux_img = IMG_TUX_CONSOLE1 + (rand() % 3);

			playsound(sounds[SND_CLICK]);
		}
      
      
		/* If Tux is being animated, show the animation: */

		if (tux_anim != -1) {
			tux_anim_frame--;

			if (tux_anim_frame < 0)
				tux_anim = -1;
			else
				tux_img = tux_anim + 1 - (tux_anim_frame / (ANIM_FRAME_START / 2));
		}


		/* Reset Tux to sitting if he's been doing nothing for a while: */

		if (old_tux_img == tux_img) {
			tux_same_counter++;

			if (tux_same_counter >= 20)
				old_tux_img = tux_img = IMG_TUX_SIT;
			if (tux_same_counter >= 60)
				old_tux_img = tux_img = IMG_TUX_RELAX1;
		} else
			tux_same_counter = 0;


		/* Handle comets: */
     
		num_comets_alive = 0;

		distanceMoved += speed;
      
		for (i = 0; i < MAX_COMETS; i++) {
			if (comets[i].alive) {

				num_comets_alive++;

				comets[i].x = comets[i].x + 0;
				comets[i].y = comets[i].y + speed;
	      
				if (comets[i].y >= (screen->h - images[IMG_CITY_BLUE]->h) && comets[i].expl == 0) {

					/* Disable shields or destroy city: */
		      
					if (cities[comets[i].city].shields) {
						cities[comets[i].city].shields = 0;
						playsound(sounds[SND_SHIELDSDOWN]);
						laser_add_score(-500 * (DIF_LEVEL+1));
					} else {
						cities[comets[i].city].expl = CITY_EXPL_START;
						playsound(sounds[SND_EXPLOSION]);
						laser_add_score(-1000 * (DIF_LEVEL+1));
					}

					tux_anim = IMG_TUX_FIST1;
					tux_anim_frame = ANIM_FRAME_START;

					/* Destroy comet: */

					comets[i].expl = COMET_EXPL_START;
				}

				/* Handle comet explosion animation: */

				if (comets[i].expl != 0) {
					comets[i].expl--;

					if (comets[i].expl == 0)
						comets[i].alive = 0;
				}
			}
		}


		/* Handle laser: */

		if (laser.alive > 0)
			laser.alive--;
     
		/* Comet time! */

		if (level_start_wait == 0 && (frame % 5) == 0 && gameover == 0) {
			if (num_attackers > 0) {

				/* More comets to add during this wave! */
		
				if ((num_comets_alive < 2 || ((rand() % 4) == 0)) && distanceMoved > 40) {
					distanceMoved = 0;
					laser_add_comet(DIF_LEVEL);
					num_attackers--;
				}
			} else {
				if (num_comets_alive == 0) {

					/* Time for the next wave! */

					/* FIXME: End of level stuff goes here */

					if (num_cities_alive > 0) {

						/* Go on to the next wave: */
						wave++;
						laser_reset_level(DIF_LEVEL);

					} else {

						/* No more cities!  Game over! */
						gameover = GAMEOVER_COUNTER_START;
					}
				}
			}
		}


		/* Handle cities: */
     
		num_cities_alive = 0;

		for (i = 0; i < NUM_CITIES; i++) 
			if (cities[i].alive) {

				num_cities_alive++;

				/* Handle animated explosion: */

				if (cities[i].expl) {
					cities[i].expl--;
		  
					if (cities[i].expl == 0)
						cities[i].alive = 0;
				}
			}
                        

		/* Handle game-over: */

		if (gameover > 0) {
			gameover--;

			if (gameover == 0)
				done = 1;
		}
                
                if ((num_cities_alive==0) && (gameover == 0))
                    gameover = GAMEOVER_COUNTER_START;
      
		/* Draw background: */
     
		SDL_BlitSurface(bkgd, NULL, screen, NULL);

		/* Draw wave: */

		dest.x = 0;
		dest.y = 0;
		dest.w = images[IMG_WAVE]->w;
		dest.h = images[IMG_WAVE]->h;

		SDL_BlitSurface(images[IMG_WAVE], NULL, screen, &dest);

		sprintf(str, "%d", wave);
		laser_draw_numbers(str, images[IMG_WAVE]->w + (images[IMG_NUMBERS]->w / 10));


		/* Draw score: */

		dest.x = (screen->w - ((images[IMG_NUMBERS]->w / 10) * 7) - images[IMG_SCORE]->w);
		dest.y = 0;
		dest.w = images[IMG_SCORE]->w;
		dest.h = images[IMG_SCORE]->h;

		SDL_BlitSurface(images[IMG_SCORE], NULL, screen, &dest);
      
		sprintf(str, "%.6d", score);
		laser_draw_numbers(str, screen->w - ((images[IMG_NUMBERS]->w / 10) * 6));
      
      
		/* Draw comets: */
      
		for (i = 0; i < MAX_COMETS; i++) 
			if (comets[i].alive) {

				/* Decide which image to display: */
				if (comets[i].expl == 0)
					img = IMG_COMET1 + ((frame + i) % 3);
				else
					img = (IMG_COMETEX2 - (comets[i].expl / (COMET_EXPL_START / 2)));
	      

				/* Draw it! */

				dest.x = comets[i].x - (images[img]->w / 2);
				dest.y = comets[i].y - images[img]->h;
				dest.w = images[img]->w;
				dest.h = images[img]->h;
	      
				SDL_BlitSurface(images[img], NULL, screen, &dest);
			}


		/* Draw letters: */

		for (i = 0; i < MAX_COMETS; i++)
			if (comets[i].alive && comets[i].expl == 0)
				laser_draw_let(comets[i].ch, comets[i].x, comets[i].y);
      
		/* Draw cities: */
      
		if (frame%2 == 0) next_frame( shield );
		for (i = 0; i < NUM_CITIES; i++) {

			/* Decide which image to display: */
	 
			if (cities[i].alive) {
				if (cities[i].expl == 0)
					img = IMG_CITY_BLUE;
				else
					img = (IMG_CITY_BLUE_EXPL5 - (cities[i].expl / (CITY_EXPL_START / 5)));
			} else 
				img = IMG_CITY_BLUE_DEAD;
	  
	  
			/* Change image to appropriate color: */
	  
			img += ((wave % MAX_CITY_COLORS) * (IMG_CITY_GREEN - IMG_CITY_BLUE));
	  
	  
			/* Draw it! */
	  
			dest.x = cities[i].x - (images[img]->w / 2);
			dest.y = (screen->h) - (images[img]->h);
			dest.w = (images[img]->w);
			dest.h = (images[img]->h);
	  
			SDL_BlitSurface(images[img], NULL, screen, &dest);

			/* Draw sheilds: */

			if (cities[i].shields) {

				dest.x = cities[i].x - (shield->frame[shield->cur]->w / 2);
				dest.h = (screen->h) - (shield->frame[shield->cur]->h);
				dest.w = src.w;
				dest.h = src.h;
				SDL_BlitSurface( shield->frame[shield->cur], NULL, screen, &dest);

			}
		}


		/* Draw laser: */

		if (laser.alive)
			laser_draw_line(laser.x1, laser.y1, laser.x2, laser.y2, 255 / (LASER_START - laser.alive),
			                192 / (LASER_START - laser.alive), 64);

		laser_draw_console_image(IMG_CONSOLE);

		if (gameover > 0)
			tux_img = IMG_TUX_FIST1 + ((frame / 2) % 2);

		laser_draw_console_image(tux_img);


		/* Draw "Game Over" */

		if (gameover > 0) {

			dest.x = (screen->w - images[IMG_GAMEOVER]->w) / 2;
			dest.y = (screen->h - images[IMG_GAMEOVER]->h) / 2;
			dest.w = images[IMG_GAMEOVER]->w;
			dest.h = images[IMG_GAMEOVER]->h;
	
			SDL_BlitSurface(images[IMG_GAMEOVER], NULL, screen, &dest);
		}
      
      
		/* Swap buffers: */
      
		SDL_Flip(screen);


		/* If we're in "PAUSE" mode, pause! */

		if (paused) {
			quit = Pause();
			paused = 0;
		}

      
		/* Keep playing music: */
      
		if (sys_sound && !Mix_PlayingMusic())
			audioMusicPlay(musics[MUS_GAME + (rand() % NUM_MUSICS)], 0);
      
		/* Pause (keep frame-rate event) */
      
		now_time = SDL_GetTicks();
		if (now_time < last_time + FPS)
			SDL_Delay(last_time + FPS - now_time);
	}
		while (!done && !quit);

  
	/* Free background: */

	if (bkgd != NULL)
		SDL_FreeSurface(bkgd);

	/* Stop music: */
	if ((sys_sound) && (Mix_PlayingMusic()))
		Mix_HaltMusic();
 
	laser_unload_data();
 
	return 1;
}


/* Reset stuff for the next level! */

void laser_reset_level(int DIF_LEVEL)
{
  unsigned char fname[1024];
  static int last_bkgd = -1;
  int i;
  
  /* Clear all comets: */
  
  for (i = 0; i < MAX_COMETS; i++)
    comets[i].alive = 0;
  
  /* Load diffrent random background image: */
  LOG("Loading background in laser_reset_level()\n");

  do {
    i = rand() % NUM_BKGDS;  /* int_rand() didn't work correctly on win32 */
    DOUT(i);
  }
  while (i == last_bkgd);

  last_bkgd = i;

  DOUT(i);

  sprintf(fname, "backgrounds/%d.jpg", i);

  LOG("Will try to load file:");
  LOG(fname);

  if (bkgd != NULL)
    SDL_FreeSurface(bkgd);

  bkgd = LoadImage(fname, IMG_REGULAR);

  if (bkgd == NULL)
  {
    fprintf(stderr,
     "\nWarning: Could not load background image:\n"
     "%s\n"
     "The Simple DirectMedia error that ocurred was: %s\n",
     fname, SDL_GetError());
  }

  /* Record score before this wave: */

  pre_wave_score = score;

  /* Set number of attackers & speed for this wave: */

  switch (DIF_LEVEL) {
    case 0 : speed = 1 + (wave/5); num_attackers=15; break;
    case 1 : speed = 1 + (wave/4); num_attackers=15; break;
    case 2 : speed = 1 + ((wave<<1)/3); num_attackers=(wave<<1); break;
    case 3 : speed = 1 + wave; num_attackers=(wave<<1); break;
    default: LOG("DIF_LEVEL not recognized!\n");
  }

  distanceMoved = 100; // so that we don't have to wait to start the level
  LOG("Leaving laser_reset_level()\n");
}


/* Add an comet to the game (if there's room): */

void laser_add_comet(int DIF_LEVEL) {

	int target, location = 0;
	static int last = -1;
	int targeted[NUM_CITIES] = { 0 };
	int add = (rand() % (DIF_LEVEL + 2));

	LOG ("Entering laser_add_comet()\n");
	DEBUGCODE { fprintf(stderr, "Adding %d comets \n", add); }

	if (0 == NUM_CITIES % 2) /* Even number of cities */
	{
          LOG("NUM_CITIES is even\n");
	  while ((add > 0) && (location != MAX_COMETS))
	  {
            /* Look for a free comet slot: */
            while ((comets[location].alive == 1) && (location < MAX_COMETS))
            {
              location++; 
            }
            if (location < MAX_COMETS)
            {
              comets[location].alive = 1;
              /* Pick a city to attack: */
              do
              { 
                target = (rand() % NUM_CITIES);
              } while (target == last || targeted[target] == 1);

              last = target;
              targeted[target] = 1;

              /* Set comet to target that city: */
              comets[location].city = target; 

              /* Start at the top, above the city in question: */
              comets[location].x = cities[target].x;
              comets[location].y = 0;

              /* Pick a letter */
              comets[location].ch = get_letter();
              add--;
            }
            DEBUGCODE {if (location == MAX_COMETS) 
			printf("Location == MAX_COMETS, we have max on screen\n");}
	  } 
	}
	else /* Odd number of cities (is this a hack that means we are using words?) */
        {
          LOG("NUM_CITIES is odd\n");
          wchar_t* word = WORDS_get();
          int i=0;

          DEBUGCODE {fprintf(stderr, "word is: %s\n", word);}
          do
          { 
  	    target = rand() % (NUM_CITIES - wcslen(word) + 1);
          } while (target == last);
          last = target;

		for (i=0; i < wcslen(word); i++)
		{
 			while ((comets[location].alive == 1) && (location < MAX_COMETS))
				location++; 

  			if (location < MAX_COMETS)
			{
				comets[location].alive = 1;
				comets[location].city = target+i; 
				comets[location].x = cities[target+i].x;
				comets[location].y = 0;
				comets[location].ch = word[i];
				DEBUGCODE {fprintf(stderr, "Assigning letter to comet: %c\n", word[i]);}
			}
		}
	}
	LOG ("Leaving laser_add_comet()\n");
}


/* Draw numbers/symbols over the attacker: */

void laser_draw_let(unsigned char c, int x, int y)
{
	SDL_Rect dst;
	dst.y = y-35;
	dst.x = x - (letters[(int)c]->w/2);
	SDL_BlitSurface(letters[(int)c], NULL, screen, &dst); 
}


/* Draw status numbers: */

void laser_draw_numbers(unsigned char * str, int x)
{
  int i, cur_x, c;
  SDL_Rect src, dest;


  cur_x = x;


  /* Draw each character: */
  
  for (i = 0; i < strlen(str); i++)
    {
      c = -1;


      /* Determine which character to display: */
      
      if (str[i] >= '0' && str[i] <= '9')
	c = str[i] - '0';
      

      /* Display this character! */
      
      if (c != -1)
	{
	  src.x = c * (images[IMG_NUMBERS]->w / 10);
	  src.y = 0;
	  src.w = (images[IMG_NUMBERS]->w / 10);
	  src.h = images[IMG_NUMBERS]->h;
	  
	  dest.x = cur_x;
	  dest.y = 0;
	  dest.w = src.w;
	  dest.h = src.h;
	  
	  SDL_BlitSurface(images[IMG_NUMBERS], &src,
			  screen, &dest);


          /* Move the 'cursor' one character width: */

	  cur_x = cur_x + (images[IMG_NUMBERS]->w / 10);
	}
    }
}

/* Draw a line: */

void laser_draw_line(int x1, int y1, int x2, int y2, int red, int grn, int blu)
{
  int dx, dy, tmp;
  float m, b;
  Uint32 pixel;
  SDL_Rect dest;
 
  pixel = SDL_MapRGB(screen->format, red, grn, blu);

  dx = x2 - x1;
  dy = y2 - y1;

  laser_putpixel(screen, x1, y1, pixel);
  
  if (dx != 0)
  {
    m = ((float) dy) / ((float) dx);
    b = y1 - m * x1;

    if (x2 > x1)
      dx = 1;
    else
      dx = -1;

    while (x1 != x2)
    {
      x1 = x1 + dx;
      y1 = m * x1 + b;
      
      laser_putpixel(screen, x1, y1, pixel);
    }
  }
  else
  {
    if (y1 > y2)
    {
      tmp = y1;
      y1 = y2;
      y2 = tmp;
    }
    
    dest.x = x1;
    dest.y = y1;
    dest.w = 3;
    dest.h = y2 - y1;

    SDL_FillRect(screen, &dest, pixel);
  }
}


/* Draw a single pixel into the surface: */

void laser_putpixel(SDL_Surface * surface, int x, int y, Uint32 pixel)
{
#ifdef PUTPIXEL_RAW
  int bpp;
  Uint8 * p;
  
  /* Determine bytes-per-pixel for the surface in question: */
  
  bpp = surface->format->BytesPerPixel;
  
  
  /* Set a pointer to the exact location in memory of the pixel
     in question: */
  
  p = (Uint8 *) (surface->pixels +       /* Start at beginning of RAM */
                 (y * surface->pitch) +  /* Go down Y lines */
                 (x * bpp));             /* Go in X pixels */
  
  
  /* Assuming the X/Y values are within the bounds of this surface... */
  
  if (x >= 0 && y >= 0 && x < surface -> w && y < surface -> h)
    {
      /* Set the (correctly-sized) piece of data in the surface's RAM
         to the pixel value sent in: */
      
      if (bpp == 1)
        *p = pixel;
      else if (bpp == 2)
        *(Uint16 *)p = pixel;
      else if (bpp == 3)
        {
          if (SDL_BYTEORDER == SDL_BIG_ENDIAN)
            {
              p[0] = (pixel >> 16) & 0xff;
              p[1] = (pixel >> 8) & 0xff;
              p[2] = pixel & 0xff;
            }
          else
            {
              p[0] = pixel & 0xff;
              p[1] = (pixel >> 8) & 0xff;
              p[2] = (pixel >> 16) & 0xff;
            }
        }
      else if (bpp == 4)
        {
          *(Uint32 *)p = pixel;
        }
    }
#else
  SDL_Rect dest;

  dest.x = x;
  dest.y = y;
  dest.w = 3;
  dest.h = 4;

  SDL_FillRect(surface, &dest, pixel);
#endif
}


/* Draw image at lower center of screen: */

void laser_draw_console_image(int i)
{
  SDL_Rect dest;

  dest.x = (screen->w - images[i]->w) / 2;
  dest.y = (screen->h - images[i]->h);
  dest.w = images[i]->w;
  dest.h = images[i]->h;

  SDL_BlitSurface(images[i], NULL, screen, &dest);
}


/* Increment score: */

void laser_add_score(int inc)
{
  score += inc;
  if (score < 0) score = 0;
}

