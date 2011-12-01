// Copyright (C) 2001-2006  Simon Baldwin (simon_baldwin@yahoo.com)
// Copyright (C) 2011       Kamil Ignacak (acerion@wp.pl)
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License along
// with this program; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
//

#include "../config.h"

#include <cstdlib>
#include <cstdio>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>

#include <sstream>
#include <iostream>
#include <iomanip>
#include <string>
#include <new>

#include <QApplication>

#include "application.h"

#include "cwlib.h"

#include "i18n.h"
#include "cmdline.h"
#include "copyright.h"
#include "dictionary.h"


//-----------------------------------------------------------------------
//  Command line mechanics
//-----------------------------------------------------------------------

namespace {
/*
bool is_console = false, is_soundcard = true, is_alsa = false;
std::string console_device, soundcard_device;
*/
cw_config_t *config = NULL;
std::string argv0;
std::string all_options = "s:|sound,d:|device,"
	"w:|wpm,t:|tone,v:|volume,"
	"g:|gap,k:|weighting,"
	"i:|infile,F:|outfile,"
	"p:|time,"
	"h|help,V|version";



// signal_handler()
//
// Signal handler, called by the CW library after its own cleanup.
void
signal_handler (int signal_number)
{
  std::clog << _("Caught signal ") << signal_number
            << _(", exiting...") << std::endl;
  exit (EXIT_SUCCESS);
}


}  // namespace


// main()
//
// Parse the command line, initialize a few things, then instantiate the
// Application and wait.
int main(int argc, char **argv)
{
	try {
	    	argv0 = program_basename(argv[0]);

		/* Set locale and message catalogs. */
		i18n_initialize();

		/* Parse combined environment and command line arguments. */
		int combined_argc;
		char **combined_argv;


		// Parse combined environment and command line arguments.  Arguments
		// are passed to QApplication() first to allow it to extract any Qt
		// or X11 options.
		combine_arguments(_("XCWCP_OPTIONS"), argc, argv, &combined_argc, &combined_argv);

		QApplication q_application (combined_argc, combined_argv);

		config = cw_config_new();
		if (!config) {
			return EXIT_FAILURE;
		}
		config->has_practice_time = 1;
		// config->has_outfile = 1;

		if (!cw_process_argv(argc, argv, all_options.c_str(), config)) {
			fprintf(stderr, _("%s: failed to parse command line args\n"), argv0.c_str());
			return EXIT_FAILURE;
		}
		if (!cw_config_is_valid(config)) {
			fprintf(stderr, _("%s: inconsistent arguments\n"), argv0.c_str());
			return EXIT_FAILURE;
		}

		if (config->input_file) {
			if (!dictionary_load(config->input_file)) {
				fprintf(stderr, _("%s: %s\n"), argv0.c_str(), strerror(errno));
				fprintf(stderr, _("%s: can't load dictionary from input file %s\n"), argv0.c_str(), config->input_file);
				return EXIT_FAILURE;
			}
		}

		if (config->output_file) {
			if (!dictionary_write(config->output_file)) {
				fprintf(stderr, _("%s: %s\n"), argv0.c_str(), strerror(errno));
				fprintf(stderr, _("%s: can't save dictionary to output file  %s\n"), argv0.c_str(), config->input_file);
				return EXIT_FAILURE;
			}
		}

		if (!cw_generator_new_from_config(config, argv0.c_str())) {
			fprintf(stderr, "%s: failed to create generator\n", argv0.c_str());
			return EXIT_FAILURE;
		}

		cw_generator_start();

		/* Set up signal handlers to clean up and exit on a range of signals. */
		struct sigaction action;
		action.sa_handler = signal_handler;
		action.sa_flags = 0;
		sigemptyset (&action.sa_mask);
		static const int SIGNALS[] = { SIGHUP, SIGINT, SIGQUIT, SIGPIPE, SIGTERM, 0 };
		for (int index = 0; SIGNALS[index] != 0; index++) {
			if (!cw_register_signal_handler(SIGNALS[index], signal_handler)) {
				perror("cw_register_signal_handler");
				abort();
			}
		}

		// Display the application's windows.
		cw::Application *application = new cw::Application ();
		application->setWindowTitle(_("Xcwcp"));
		application->show();
		q_application.connect(&q_application, SIGNAL (lastWindowClosed ()),
				      &q_application, SLOT (quit ()));

		// Enter the application event loop.
		int rv = q_application.exec();

		cw_generator_stop();
		cw_generator_delete();
		return rv;
	}

	// Handle any exceptions thrown by the above.
	catch (std::bad_alloc) {
			std::clog << "Internal error: heap memory exhausted" << std::endl;
			return EXIT_FAILURE;
	}
	catch (...) {
		std::clog << "Internal error: unknown problem" << std::endl;
		return EXIT_FAILURE;
	}
}
