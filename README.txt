Field Encrypt
-------------

The goal of this module is to create a method for encrypting field values when
stored in the database.

This module depends on the Encrypt module (http://drupal.org/project/encrypt)
for encrypting and decrypting strings.

Install this module like any other module. Field encryption settings are made on
the field edit page.



Memcache support
----------------

To enable Memcache support for the Field Encryption module you have to ensure
that the Memcache Field Encryption Cache backend is enabled. This can be set via
Drupal's Performance settings form.