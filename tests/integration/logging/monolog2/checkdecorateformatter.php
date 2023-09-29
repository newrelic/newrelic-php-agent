<?php
/* create formatter class that echos the interesting data that the log 
   decoration should have added. */
class CheckDecorateFormatter implements Monolog\Formatter\FormatterInterface {
  public function __construct(?string $dateFormat = null) {
  }
  public function format(array $record) {
    $nrlinking = $record['extra']['NR-LINKING'] ?? 'NR-LINKING DATA MISSING!!!';
    $result = preg_match("/(NR\-LINKING)\|([\w\d]+)\|([\w\d]+)\|([\w\d]+)\|([\w\d]+)\|([\w\d\%]+\.php)\|/", $nrlinking, $matches);
    $linkmeta = newrelic_get_linking_metadata();

    tap_equal(7, count($matches), "All NR-LINKING elements present");
    if (7 == count($matches)) {
      tap_equal("NR-LINKING", $matches[1], "NR-LINKING present");
      tap_equal($linkmeta['entity.guid'] ?? '<missing entity.guid>', $matches[2], "entity.guid correct");
      tap_equal($linkmeta['hostname'] ?? '<missing hostname>', $matches[3], "hostname correct");
      tap_equal($linkmeta['trace.id'] ?? '<missing trace.id>', $matches[4], "trace.id correct");
      tap_equal(true, strlen($matches[5]) > 0 && preg_match("/[\w\d]+/",$matches[5]), "span.id is non-zero length and alphanumeric");
      if (isset($linkmeta['entity.name'])) {
        $name = urlencode($linkmeta['entity.name']);
      } else {
        $name = '<missing entity.name>';
      }
      tap_equal($name, $matches[6], "entity.name correct");
    }

    /* have to return a non-null value which is the output string */
    return "";
  }

  public function formatBatch(array $records) {
    foreach ($records as $key => $record) {
      $records[$key] = $this->format($record);
    }

    return $records;  
  }
}
