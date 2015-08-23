<?php

/**
 * Class KAVALogin_AccountAPI
 * Functies om te communiceren met de account/password API van KAVA.
 */
class KAVALogin_AccountAPI {

  private static $instance;

  /**
   * Get instance
   * @return bool|\KAVALogin_AccountAPI Instance
   */
  public static function getInstance() {
    if (!self::$instance) {
      self::$instance = new self;
    }

    return self::$instance;
  }

  /**
   * Creates a new CAS account for a new user
   * @param string $email Email Address
   * @param string $firstName First Name
   * @param string $lastName Last Name
   * @return mixed|bool Result or success
   */
  public function createAccount($email, $firstName = '', $lastName = '') {

    $apiUrl = $this->getApiUrl('create', ['email' => $email, 'naam' => $lastName, 'voornaam' => $firstName]);

    if(!$apiUrl) {
      kavalogin_log('Can\'t create account: invalid or incomplete parameters.', WATCHDOG_ERROR);
      return false;
    }

    $ret = drupal_http_request($apiUrl);
    drupal_set_message('Calling API: ' . $apiUrl . '.<br>Result: HTTP ' . $ret->code . ': ' . print_r($ret->data, true), 'warning');

    return $ret;
  }

  /**
   * Creates a new CAS account for an existing CRM contact
   * @param string $barcode Barcode
   * @return mixed|bool Result or success
   */
  public function createAccountForContact($barcode) {

    $apiUrl = $this->getApiUrl('account', ['barcode' => $barcode]);

    if(!$apiUrl) {
      kavalogin_log('Can\'t create account: invalid or incomplete parameters.', WATCHDOG_ERROR);
      return false;
    }

    $ret = drupal_http_request($apiUrl);
    drupal_set_message('Calling API: ' . $apiUrl . '.<br>Result: HTTP ' . $ret->code . ': ' . print_r($ret->data, true), 'warning');

    return $ret;
  }

  /**
   * Get full API url with parameters
   * @param string $action API action
   * @param array $params API GET parameters
   * @return bool|string URL or false
   * @throws \Exception If API url or key is not set
   */
  private function getApiUrl($action, $params = []) {

    $apiUrl = variable_get('kavalogin_apiurl');
    $apiKey = variable_get('kavalogin_apikey');
    if(!$apiUrl || !$apiKey) {
      throw new \Exception('KAVA account API url and key not set.');
    }

    if(substr($apiUrl, -1) != '/') {
      $apiUrl .= '/';
    }

    switch($action) {

      case 'create':
        // create?naam=...&voornaam=...&email=...&signature=<email+key>
        if(!isset($params['email'])) {
          return false;
        }
        $params['signature'] = md5($apiKey . $params['email']);
        break;

      case 'account':
        // account?barcode=...&signature=<barcode+key>
        if(!isset($params['barcode'])) {
          return false;
        }
        $params['signature'] = md5($apiKey . $params['barcode']);
        break;

      default:
        // Pass on without checks or changes
    }

    $apiUrl .= $action . '?' . http_build_query($params);
    return $apiUrl;
  }

  /**
   * Log helper function (shortcut for watchdog / drupal_set_message)
   * @param string $message Message
   * @param bool|FALSE $display Display message
   * @param int $severity Severity (WATCHDOG_*)
   */
  private function log($message, $display = FALSE, $severity = WATCHDOG_WARNING) {
    if ($display) {
      drupal_set_message('KAVALogin: ' . $message, 'warning');
    }
    watchdog('kavalogin', $message, [], $severity);
  }

}