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
   * Call the KAVA account API. Will check if a user exists and if not, automatically create a user account.
   * @param string $email Email Address
   * @param string $firstName First Name
   * @param string $lastName Last Name
   * @return mixed|bool Result or success
   */
  public function callAccountAPI($email, $firstName = '', $lastName = '') {

    $apiUrl = $this->getApiUrl('create', [
      'email'    => $email,
      'naam'     => strtoupper($lastName),
      'voornaam' => strtoupper($firstName),
    ]);

    $ret = drupal_http_request($apiUrl);
    kavalogin_log("Calling API: " . $apiUrl . ".\nResult: HTTP " . $ret->code . ": " . print_r($ret->data, TRUE), false, WATCHDOG_INFO);

    if(!$ret || $ret->code != 200 || !$ret->data) {
      return false;
    }

    $data = json_decode($ret->data);
    if(!$data) {
      return false;
    }

    return $data;
  }

  /**
   * Get full API url with parameters
   * @param string $action API action
   * @param array $params API GET parameters
   * @return bool|string URL or false
   * @throws \Exception If API url or key is not set
   */
  private function getApiUrl($action = 'create', $params = []) {

    $apiUrl = variable_get('kavalogin_apiurl');
    $apiKey = variable_get('kavalogin_apikey');
    if (!$apiUrl || !$apiKey) {
      throw new \Exception('KAVA account API url and key not set.');
    }

    if (substr($apiUrl, -1) != '/') {
      $apiUrl .= '/';
    }

    if (empty($params['email'])) {
      throw new \Exception('KAVA account API: no email address provided.');
    }

    $params['signature'] = md5($apiKey . $params['email']);

    // Generate API URL. URL encoding voor email wordt niet ondersteund... dus geen http_build_query
    $apiUrl .= $action . '?';
    foreach ($params as $key => $value) {
      if ($key != 'email') {
        $value = rawurlencode($value);
      }
      $apiUrl .= $key . '=' . $value . '&';
    }
    return $apiUrl;
  }

}