<?php
/**
 * This is an include file.  These are the "callback" functions that drush
 * uses to get the job done.  See quickstart.drush.inc for the definition of
 * the commands.
 *
 * Example make file for d7 http://drupal.org/node/852412
 *
 * site-install issues:
 *  - http://drupal.org/node/992928 - Drupal Core
 *  - http://drupal.org/node/983914 - Drush
 * @file
 */

/**
 * Helper function for executing shell commands.  Throws exceptions on error.
 *
 * @param <type> $cmd the command to run
 * @param <type> $throwexception throw exception on error?
 */
function quickstart_shell_exec($cmd, $throwexception=true) {
  // Note: for shell commands: 0=success, non-zero=error code
  $ret = drush_shell_exec($cmd);
  if ($throwexception && !$ret) {
    $msg = "Command returned unexpected result: $cmd";
    $output = drush_shell_exec_output();
    foreach ($output as $line) {
      $msg="\n  $line";
    }
    throw new Exception($msg);
  }
}
/**
 * Helped function for finding a string in a file and return
 * a count larger than one if it exists
 * 
 * @param $what What to look for
 * @param $where Where to look for $what
 *
 * @return 1 or larger if we get a match else 0
 */
function quickstart_match($what, $where) { 
  //Load hosts file
  $x=file_get_contents($where); 

  //Create array and search for our string
  $a=explode("\n",$x); 
  $b=preg_grep("/$what/",$a); 

  //return matches. 0 if none and 1 and above if any
  if(count($b) >= 1) {
    return 1;
  } else {
    return 0;
  }
}

/**
 * Helper function for running SQL on a local mysql database.
 *
 * @param <type> $sql
 */
function quickstart_mysql_exec($sql, $dbuser, $dbpassword) {
  quickstart_shell_exec("echo \"$sql\" | mysql -u$dbuser -p$dbpassword");
}

/**
 * Helper function for dereferencing paths. Working on both Linux and Mac
 * using PHP_OS built-in
 *
 * @param <type> $pathin
 * @return <type>
 */
function quickstart_fixpath($pathin) {
  // fix ~'s
  $home_str = "";

  switch(PHP_OS) {
    //Ubuntu etc
    case "Linux":
      $home_str = "home";
      break;

    //Mac
    case "Darwin":
      $home_str = "Users";
      break;

    default:
      $home_str = "";
  }

  $homedir = "/$home_str/" . exec('whoami');
  $pathout = str_replace("~", $homedir, $pathin);
  // realpath fails on non-existant folders.  So it's hard to fix .'s and ..'s
  return $pathout;
}

/**
 * Create the DNS.  Edits /etc/hosts on local machine.
 * Entries in file are marked with #quickstart
 * If the entry already eixsts, it will not add a new one 
 * to avoid duplicate entries in the hosts file
 *
 * @param <type> $domain the domain to DNS
 * @param <type> $params
 */
function quickstart_create_dns($domain, $params) {
  $dns_entry = quickstart_match($domain, "/etc/hosts");
  if($dns_entry === 0) {  
    //Entry doesn't exist in hosts file. Create it.
    quickstart_shell_exec("echo '127.0.0.1 $domain #quickstart' | sudo tee -a /etc/hosts");
  }
}

/**
 * Destroy the DNS.  Edit /etc/hosts file on local machine
 *
 * @param <type> $domain
 * @param <type> $params
 */
function quickstart_destroy_dns($domain, $params) {
  quickstart_shell_exec("sudo sed -i_bak -e '/".$domain."/d' /etc/hosts");
}


/**
 * This is the virtual hosts template file for the apache functions.
 */
define('APACHE_TEMPLATE', "
<VirtualHost *:80>
  ServerName #DOMAIN#
  ServerAlias *.#DOMAIN#
  DocumentRoot #CODEPATH#
  <Directory #CODEPATH#>
    Options Indexes FollowSymLinks MultiViews
    AllowOverride All
    Order allow,deny
    allow from all
  </Directory>
</VirtualHost>
<IfModule mod_ssl.c>
  <VirtualHost *:443>
    ServerName #DOMAIN#
    ServerAlias *.#DOMAIN#
    DocumentRoot #CODEPATH#
    <Directory #CODEPATH#>
      Options Indexes FollowSymLinks MultiViews
      AllowOverride All
      Order allow,deny
      allow from all
    </Directory>
    SSLEngine on
    SSLCertificateFile    /etc/ssl/certs/ssl-cert-snakeoil.pem
    SSLCertificateKeyFile /etc/ssl/private/ssl-cert-snakeoil.key
  </VirtualHost>
</IfModule>");

/**
 * Configure apache virtualhost file using the template.
 * Files put in /etc/apache2/sites-available/$domain, then a2ensite'd
 * Runs: apache2ctl restart
 *
 * @param <type> $domain
 * @param <type> $params 'codepath'
 */
function quickstart_create_apache($domain, $params) {
  // get codepath and make sure it exists.
  $codepath = $params['codepath'];

  if (!file_exists($codepath)) mkdir($codepath, 0777, true);

  // get template
  $str_config = APACHE_TEMPLATE;

  // make our own :-)
  $str_config = str_replace("#DOMAIN#", $domain, $str_config);
  $str_config = str_replace("#CODEPATH#", $codepath, $str_config);

  $config_handle = fopen("/etc/apache2/sites-available/$domain", "w+");
  fwrite($config_handle, $str_config);

  // restart apache
  quickstart_shell_exec("sudo apachectl restart");
}

/**
 * Destroy the apache virtual hosts config. a2dissite, then rm file.
 * Runs: apache2ctl restart
 *
 * @param <type> $domain
 * @param <type> $params
 */
function quickstart_destroy_apache($domain, $params) {
  $filename = "/etc/apache2/sites-available/$domain";
  if (file_exists($filename)) unlink($filename);  // unlink = delete file
  quickstart_shell_exec("sudo apachectl restart");
}

/**
 * These define SQL commands to run on database.  CREATE USER causes trouble.  Note GRANT ALL ON *.* for site-install
 */
define('DATABASE_CREATE', "
GRANT USAGE 
  ON *.* TO #DOMAIN#@localhost 
  IDENTIFIED BY '#DOMAIN#' 
  WITH  MAX_QUERIES_PER_HOUR 0 
        MAX_CONNECTIONS_PER_HOUR 0 
        MAX_UPDATES_PER_HOUR 0 
        MAX_USER_CONNECTIONS 0;
CREATE DATABASE IF NOT EXISTS #DOMAIN#;
GRANT ALL PRIVILEGES 
  ON *.* TO #DOMAIN#@localhost;
FLUSH PRIVILEGES;");
/**
 * We GRANT before DROP USER to make sure there is something to drop.  USAGE is synonym for 'no priveledges'.
 */
define('DATABASE_DESTROY', "
GRANT USAGE 
  ON *.* TO #DOMAIN#@localhost;
DROP USER #DOMAIN#@localhost;
DROP DATABASE IF EXISTS #DOMAIN#;
FLUSH PRIVILEGES;");

/**
 * Create the database.  Runs SQL.  Replaces .'s with _'s in domain name
 * to get the database name, database username, and password.
 *
 * @param <type> $domain
 * @param <type> $params
 */
function quickstart_create_database($domain, $params) {
  $dbuser = $params['dbuser'];
  $dbpassword = $params['dbpassword'];
  $domain_ = $params['domain_'];

  $sql = str_replace("#DOMAIN#", $domain_, DATABASE_CREATE);
  quickstart_mysql_exec($sql, $dbuser, $dbpassword);
}
/**
 * Destroys the database using SQL.
 *
 * @param <type> $domain
 * @param <type> $params
 */
function quickstart_destroy_database($domain, $params) {
  $dbuser = $params['dbuser'];
  $dbpassword = $params['dbpassword'];
  $domain_ = $params['domain_'];

  $sql = str_replace("#DOMAIN#", $domain_, DATABASE_DESTROY);
  quickstart_mysql_exec($sql, $dbuser, $dbpassword);
}

/**
 * Download the code using drush make.  
 * If path already exists, moves it to path.old.
 * 
 * @param <type> $domain
 * @param <type> $params 'codepath' 'makefile'
 */
function quickstart_create_code($domain, $params) {
  $codepath = $params['codepath'];
  $makefile = $params['makefile'];

  if (file_exists($codepath)) {
    quickstart_shell_exec("mv $codepath $codepath.old");
    drush_log("Moved existing directory $codepath to $codepath.old", "ok");
  }
  if (!file_exists($makefile)) {
    // try to find makefile in ~/websites folder
    $makefile = quickstart_fixpath("$makefile");
    if (!file_exists($makefile)) throw new Exception("Makefile ". $makefile ." not found.");
  }

  drush_log("Starting to run make command","notice");

  //Using drush_invoke_process for Drush 5.x usage
  //Make
  drush_invoke_process(
    @self, 
    "make", 
    array(
      '--prepare-install',
      '--yes'
    ), 
    array(
      $makefile,
      $codepath
    )
  );
  //Quickstart fixperms
  drush_invoke_process(
    @self, 
    "quickstart-fixperms", 
    null, 
    array(
      "--domain=".$domain,
      "--codepath=".$codepath,
      "--yes"
    )
  );
}

/**
 * Deletes the sites files using rm -rf.  Also path.old.
 *
 * @param <type> $domain
 * @param <type> $params 'codepath'
 */
function quickstart_destroy_code($domain, $params) {
  $codepath = $params['codepath'];
  // only delete a web folder.  "drush qd --codepath=/" would be bad.
  if (!file_exists($codepath . '/index.php')) {
      drush_log('No index.php found.  Aborting deletion of files...', 'warning');
      return;
  }
  quickstart_shell_exec("sudo rm -rf $codepath");
}

/**
 * Install the site.  D6 and D7 versions.
 *
 * @param <type> $domain
 * @param <type> $params
 */
function quickstart_create_install($domain, $params) {
  // detect the drupal version from the CHANGELOG.txt file.
  $dversion = 'unknown';
  $codepath = $params['codepath'];

  // load make file into string
  $str=file_get_contents($codepath . '/CHANGELOG.txt');
  if ( preg_match("/^Drupal 6./m", $str) ) {
    $dversion = 'd6';
  }
  // regex magic.
  if ( preg_match("/^Drupal 7./m", $str) ) {
    $dversion = 'd7';
  }
  
  // D6 specific
  if ($dversion=='d6') {
    // D6 and D7 have different names for the default profile.
    if ($params['profile']=='standard') {
      $params['profile'] = "default";
    }
    // copy default.settings.php to settings.php and change URL and prefix
    $str = file_get_contents("$codepath/sites/default/default.settings.php");
    $str = str_replace("mysql://username:password@localhost/databasename",
          $params['db-url'],
          $str);
    $str = str_replace("\$db_prefix = ''",
          $params['db-prefix'],
          $str);
    file_put_contents("$codepath/sites/".$params['sites-subdir']."/settings.php",$str);
  }

  //Install site
  drush_log(
  drush_invoke_process(
    @self,
    "site-install ",
    array(
      $params['profile']
    ),
    array(
      "--root=".$params['codepath'],
      "--db-url=".$params['db-url'],
      "--db-prefix=".$params['db-prefix'],
      "--account-name=".$params['account-name'],
      "--account-pass=".$params['account-pass'],
      "--account-mail=".$params['account-mail'],
      "--locale=".$params['locale'],
      "--clean-url=".$params['clean-url'],
      "--site-name=".$params['site-name'],
      "--site-mail=".$params['site-mail'],
      "--site-subdir=".$params['sites-subdir'],
    )
  )
  , "ok");

  // D7 specific
  if ($dversion=='d7') {
    // Create a private files folder.  This is done to make fix-perms not break configuring a private files directory
    quickstart_shell_exec("sudo mkdir $codepath/sites/default/files_private");
    
    //Quickstart fixperms
    drush_invoke_process(
      @self, 
      "quickstart-fixperms", 
      null, 
      array(
        '--domain='.$domain,
        '--codepath='.$codepath
      )
    );
  }

  drush_log("\nURL:  http://".$params['domain']."\nAdmin user:  ".$params['account-name']."\nAdmin password: ".$params['account-pass'], 'ok');

  //Open new drupal project in the browser
  quickstart_shell_exec("open http://".$params['domain']);

  //Open the project in sublime text 2 and start working!
  quickstart_shell_exec("subl ".$params['codepath']);
}

/**
 * Return site to situation where "drush qc install --domain=site.dev" could
 * be re-run successfully.
 *
 * @param <type> $domain
 * @param <type> $params 'codepath'
 */
function quickstart_destroy_install($domain, $params) {
  // only delete a web folder.  "drush qd --codepath=/" would be bad.
  $codepath = $params['codepath'];
  if (!file_exists($codepath . '/index.php') && $codepath != "/") {
      drush_log('No index.php found.  Aborting un-install of site...', 'warning');
      return;
  }

  // drop and create database and user.
  quickstart_destroy_database($domain, $params);
  quickstart_create_database($domain, $params);

  // delete settings.php file
  quickstart_shell_exec("sudo rm -f $codepath/sites/all/default/settings.php");

  // delete files folder
  quickstart_shell_exec("sudo rm -rf $codepath/sites/all/default/files");
}