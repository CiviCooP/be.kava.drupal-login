KAVA CAS Login (be.kava.drupal-login)
==========================================================

Extra functionaliteit voor de login op kava.be:
  
- Drupal-user en CiviCRM-contact worden gelinkt gebaseerd op de aanwezigheid van een 'KAVA-accountlogin' bij het contact in CiviCRM
- Custom registratie-formulier op /account/registreren maakt een account aan op de CAS-server, en indien nodig ook een Drupal-gebruiker en CiviCRM-contact
- Wachtwoord wijzigen-formulier op /account/changepass
  
Gezien de uitbreidingen in deze module is het wellicht een goed idee deze in de toekomst nog om te bouwen om beter te integreren met de standaard Drupal-registratie en wachtwoord-functie (/user/register e.d.)
