#include <libmodest-dbus-client/libmodest-dbus-client.h>
#include <stdio.h>

int main(int argc, char *argv[])
{
	/* Initialize maemo application */
	osso_context_t * osso_context = osso_initialize(
	    "test_hello", "0.0.1", TRUE, NULL);
	       
	/* Check that initialization was ok */
	if (osso_context == NULL)
	{
		printf("osso_initialize() failed.\n");
	    return OSSO_ERROR;
	}
	
	/* Call the function in libmodest-dbus-client: */
	/* TODO: The Message URI system is not yet implemented. */
	const gboolean ret = libmodest_dbus_client_open_message (osso_context,
		"http://todo_some_message_uri");
	if (!ret) {
			printf("libmodest_dbus_client_open_message() failed.\n");
		return OSSO_ERROR;
	} else {
		printf("libmodest_dbus_client_open_message() succeeded.\n");
	}
		
    /* Exit */
    return 0;
}
