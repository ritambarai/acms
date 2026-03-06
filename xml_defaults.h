#pragma once

/* XML defaults embedded at build time by codegen.py.
 * Call provision_spiffs_xml() once in setup() after SPIFFS.begin()
 * to ensure these files are always present after flashing. */

static const char METADATA_XML_DEFAULT[] = R"XMLRAW(
<Metadata>
<row><Class>Fault_Code</Class><Key>101</Key><Message>Fault</Message></row>
<row><Class>Fault_Code</Class><Key>201</Key><Message>Low Level</Message></row>
<row><Class>Fault_Code</Class><Key>301</Key><Message>Over Voltage</Message></row>
<row><Class>Fault_Code</Class><Key>302</Key><Message>Under Voltage</Message></row>
<row><Class>Fault_Code</Class><Key>401</Key><Message>Low Pressure</Message></row>
<row><Class>Fault_Code</Class><Key>402</Key><Message>High Pressure</Message></row>
<row><Class>Function_ID</Class><Key>1</Key><Message>Coils</Message></row>
<row><Class>Function_ID</Class><Key>2</Key><Message>Discrete Inputs</Message></row>
<row><Class>Function_ID</Class><Key>3</Key><Message>Holding Registers</Message></row>
<row><Class>Function_ID</Class><Key>4</Key><Message>Input Registers</Message></row>
<row><Class>level</Class><Key>1</Key><Message>Percentage</Message></row>
<row><Class>level</Class><Key>2</Key><Message>meter</Message></row>
<row><Class>mode</Class><Key>0</Key><Message>AUTO</Message></row>
<row><Class>mode</Class><Key>1</Key><Message>MANUAL</Message></row>
<row><Class>Operation_ID</Class><Key>0</Key><Message>Equals to</Message></row>
<row><Class>Operation_ID</Class><Key>1</Key><Message>Less than</Message></row>
<row><Class>Operation_ID</Class><Key>2</Key><Message>Greater than</Message></row>
<row><Class>Operation_ID</Class><Key>3</Key><Message>Greater than/ Equals to</Message></row>
<row><Class>Operation_ID</Class><Key>4</Key><Message>Less than/ Equals to</Message></row>
<row><Class>Operation_ID</Class><Key>5</Key><Message>Not Equals to</Message></row>
<row><Class>pressure</Class><Key>1</Key><Message>bar</Message></row>
<row><Class>state</Class><Key>0</Key><Message>OFF</Message></row>
<row><Class>state</Class><Key>1</Key><Message>ON</Message></row>
<row><Class>status</Class><Key>0</Key><Message>OK</Message></row>
<row><Class>status</Class><Key>1</Key><Message>FAULT</Message></row>
<row><Class>trip</Class><Key>0</Key><Message>UNTRIPED</Message></row>
<row><Class>trip</Class><Key>1</Key><Message>TRIPED</Message></row>
<row><Class>type</Class><Key>0</Key><Message>choice</Message></row>
<row><Class>type</Class><Key>1</Key><Message>numeric</Message></row>
<row><Class>voltage</Class><Key>1</Key><Message>Volts</Message></row>
</Metadata>
)XMLRAW";

static const char VARIABLES_XML_DEFAULT[] = R"XMLRAW(
<Variables>
<row><Class>air pressure switch</Class><Name>status</Name><Category>type</Category><Value>0</Value><Slave_ID/><Function_ID/><Start_Address/><Data_Length/><Operation_ID/><Threshold/><Fault_Code/><Increment/></row>
<row><Class>air pressure switch</Class><Name>status</Name><Category>First Floor</Category><Value>0</Value><Slave_ID/><Function_ID/><Start_Address/><Data_Length/><Operation_ID>0</Operation_ID><Threshold>1</Threshold><Fault_Code>101</Fault_Code><Increment/></row>
<row><Class>air pressure switch</Class><Name>status</Name><Category>Fourth Floor</Category><Value>0</Value><Slave_ID/><Function_ID/><Start_Address/><Data_Length/><Operation_ID>0</Operation_ID><Threshold>1</Threshold><Fault_Code>101</Fault_Code><Increment/></row>
<row><Class>air pressure switch</Class><Name>status</Name><Category>Ground Floor</Category><Value>0</Value><Slave_ID/><Function_ID/><Start_Address/><Data_Length/><Operation_ID>0</Operation_ID><Threshold>1</Threshold><Fault_Code>101</Fault_Code><Increment/></row>
<row><Class>air pressure switch</Class><Name>status</Name><Category>Second Floor</Category><Value>0</Value><Slave_ID/><Function_ID/><Start_Address/><Data_Length/><Operation_ID>0</Operation_ID><Threshold>1</Threshold><Fault_Code>101</Fault_Code><Increment/></row>
<row><Class>air pressure switch</Class><Name>status</Name><Category>Third Floor</Category><Value>0</Value><Slave_ID/><Function_ID/><Start_Address/><Data_Length/><Operation_ID>0</Operation_ID><Threshold>1</Threshold><Fault_Code>101</Fault_Code><Increment/></row>
<row><Class>diesel tanks</Class><Name>level</Name><Category>type</Category><Value>1</Value><Slave_ID/><Function_ID/><Start_Address/><Data_Length/><Operation_ID/><Threshold/><Fault_Code/><Increment/></row>
<row><Class>diesel tanks</Class><Name>level</Name><Category>unit</Category><Value>1</Value><Slave_ID/><Function_ID/><Start_Address/><Data_Length/><Operation_ID/><Threshold/><Fault_Code/><Increment/></row>
<row><Class>diesel tanks</Class><Name>level</Name><Category>main</Category><Value>3000</Value><Slave_ID>1</Slave_ID><Function_ID>4</Function_ID><Start_Address>69</Start_Address><Data_Length>1</Data_Length><Operation_ID>4</Operation_ID><Threshold>10</Threshold><Fault_Code>201</Fault_Code><Increment/></row>
<row><Class>manual call point</Class><Name>status</Name><Category>type</Category><Value>0</Value><Slave_ID/><Function_ID/><Start_Address/><Data_Length/><Operation_ID/><Threshold/><Fault_Code/><Increment/></row>
<row><Class>manual call point</Class><Name>status</Name><Category>First Floor</Category><Value>0</Value><Slave_ID/><Function_ID/><Start_Address/><Data_Length/><Operation_ID>0</Operation_ID><Threshold>1</Threshold><Fault_Code>101</Fault_Code><Increment/></row>
<row><Class>manual call point</Class><Name>status</Name><Category>Fourth Floor</Category><Value>0</Value><Slave_ID/><Function_ID/><Start_Address/><Data_Length/><Operation_ID>0</Operation_ID><Threshold>1</Threshold><Fault_Code>101</Fault_Code><Increment/></row>
<row><Class>manual call point</Class><Name>status</Name><Category>Ground Floor</Category><Value>0</Value><Slave_ID/><Function_ID/><Start_Address/><Data_Length/><Operation_ID>0</Operation_ID><Threshold>1</Threshold><Fault_Code>101</Fault_Code><Increment/></row>
<row><Class>manual call point</Class><Name>status</Name><Category>Second Floor</Category><Value>0</Value><Slave_ID/><Function_ID/><Start_Address/><Data_Length/><Operation_ID>0</Operation_ID><Threshold>1</Threshold><Fault_Code>101</Fault_Code><Increment/></row>
<row><Class>manual call point</Class><Name>status</Name><Category>Third Floor</Category><Value>0</Value><Slave_ID/><Function_ID/><Start_Address/><Data_Length/><Operation_ID>0</Operation_ID><Threshold>1</Threshold><Fault_Code>101</Fault_Code><Increment>0</Increment></row>
<row><Class>pipes</Class><Name>pressure</Name><Category>type</Category><Value>1</Value><Slave_ID/><Function_ID/><Start_Address/><Data_Length/><Operation_ID/><Threshold/><Fault_Code/><Increment/></row>
<row><Class>pipes</Class><Name>pressure</Name><Category>unit</Category><Value>1</Value><Slave_ID/><Function_ID/><Start_Address/><Data_Length/><Operation_ID/><Threshold/><Fault_Code/><Increment/></row>
<row><Class>pipes</Class><Name>pressure</Name><Category>booster</Category><Value>5</Value><Slave_ID/><Function_ID/><Start_Address/><Data_Length/><Operation_ID>1</Operation_ID><Threshold>1</Threshold><Fault_Code>401</Fault_Code><Increment/></row>
<row><Class>pipes</Class><Name>pressure</Name><Category>fire hose</Category><Value>5</Value><Slave_ID/><Function_ID/><Start_Address/><Data_Length/><Operation_ID>1</Operation_ID><Threshold>1</Threshold><Fault_Code>401</Fault_Code><Increment/></row>
<row><Class>pipes</Class><Name>pressure</Name><Category>hydrant</Category><Value>5</Value><Slave_ID>1</Slave_ID><Function_ID>4</Function_ID><Start_Address>66</Start_Address><Data_Length>1</Data_Length><Operation_ID>1</Operation_ID><Threshold>1</Threshold><Fault_Code>401</Fault_Code><Increment/></row>
<row><Class>pipes</Class><Name>pressure</Name><Category>main line</Category><Value>5</Value><Slave_ID/><Function_ID/><Start_Address/><Data_Length/><Operation_ID>1</Operation_ID><Threshold>1</Threshold><Fault_Code>401</Fault_Code><Increment/></row>
<row><Class>pipes</Class><Name>pressure</Name><Category>sprinkler</Category><Value>5</Value><Slave_ID>1</Slave_ID><Function_ID>4</Function_ID><Start_Address>67</Start_Address><Data_Length>1</Data_Length><Operation_ID>1</Operation_ID><Threshold>1</Threshold><Fault_Code>401</Fault_Code><Increment/></row>
<row><Class>pipes</Class><Name>pressure</Name><Category>stand pipe</Category><Value>5</Value><Slave_ID/><Function_ID/><Start_Address/><Data_Length/><Operation_ID>1</Operation_ID><Threshold>1</Threshold><Fault_Code>401</Fault_Code><Increment/></row>
<row><Class>pumps</Class><Name>mode</Name><Category>type</Category><Value>0</Value><Slave_ID/><Function_ID/><Start_Address/><Data_Length/><Operation_ID/><Threshold/><Fault_Code/><Increment/></row>
<row><Class>pumps</Class><Name>mode</Name><Category>booster</Category><Value>0</Value><Slave_ID>1</Slave_ID><Function_ID>2</Function_ID><Start_Address>9</Start_Address><Data_Length>1</Data_Length><Operation_ID/><Threshold/><Fault_Code/><Increment/></row>
<row><Class>pumps</Class><Name>mode</Name><Category>diesel</Category><Value>0</Value><Slave_ID/><Function_ID/><Start_Address/><Data_Length/><Operation_ID/><Threshold/><Fault_Code/><Increment/></row>
<row><Class>pumps</Class><Name>mode</Name><Category>domestic</Category><Value>0</Value><Slave_ID>1</Slave_ID><Function_ID>2</Function_ID><Start_Address>13</Start_Address><Data_Length>1</Data_Length><Operation_ID/><Threshold/><Fault_Code/><Increment/></row>
<row><Class>pumps</Class><Name>mode</Name><Category>jockey</Category><Value>0</Value><Slave_ID>1</Slave_ID><Function_ID>2</Function_ID><Start_Address>5</Start_Address><Data_Length>1</Data_Length><Operation_ID/><Threshold/><Fault_Code/><Increment/></row>
<row><Class>pumps</Class><Name>mode</Name><Category>main</Category><Value>0</Value><Slave_ID>1</Slave_ID><Function_ID>2</Function_ID><Start_Address>1</Start_Address><Data_Length>1</Data_Length><Operation_ID/><Threshold/><Fault_Code/><Increment/></row>
<row><Class>pumps</Class><Name>state</Name><Category>type</Category><Value>0</Value><Slave_ID/><Function_ID/><Start_Address/><Data_Length/><Operation_ID/><Threshold/><Fault_Code/><Increment/></row>
<row><Class>pumps</Class><Name>state</Name><Category>booster</Category><Value>1</Value><Slave_ID>1</Slave_ID><Function_ID>1</Function_ID><Start_Address>1002</Start_Address><Data_Length>1</Data_Length><Operation_ID/><Threshold/><Fault_Code/><Increment/></row>
<row><Class>pumps</Class><Name>state</Name><Category>diesel</Category><Value>0</Value><Slave_ID/><Function_ID/><Start_Address/><Data_Length/><Operation_ID/><Threshold/><Fault_Code/><Increment/></row>
<row><Class>pumps</Class><Name>state</Name><Category>domestic</Category><Value>0</Value><Slave_ID>1</Slave_ID><Function_ID>1</Function_ID><Start_Address>1003</Start_Address><Data_Length>1</Data_Length><Operation_ID/><Threshold/><Fault_Code/><Increment/></row>
<row><Class>pumps</Class><Name>state</Name><Category>jockey</Category><Value>0</Value><Slave_ID>1</Slave_ID><Function_ID>1</Function_ID><Start_Address>1001</Start_Address><Data_Length>1</Data_Length><Operation_ID/><Threshold/><Fault_Code/><Increment/></row>
<row><Class>pumps</Class><Name>state</Name><Category>main</Category><Value>1</Value><Slave_ID>1</Slave_ID><Function_ID>1</Function_ID><Start_Address>1000</Start_Address><Data_Length>1</Data_Length><Operation_ID/><Threshold/><Fault_Code/><Increment/></row>
<row><Class>pumps</Class><Name>trip</Name><Category>type</Category><Value>0</Value><Slave_ID/><Function_ID/><Start_Address/><Data_Length/><Operation_ID/><Threshold/><Fault_Code/><Increment/></row>
<row><Class>pumps</Class><Name>trip</Name><Category>booster</Category><Value>0</Value><Slave_ID>1</Slave_ID><Function_ID>2</Function_ID><Start_Address>8</Start_Address><Data_Length>1</Data_Length><Operation_ID>0</Operation_ID><Threshold>1</Threshold><Fault_Code>101</Fault_Code><Increment/></row>
<row><Class>pumps</Class><Name>trip</Name><Category>diesel</Category><Value>0</Value><Slave_ID/><Function_ID/><Start_Address/><Data_Length/><Operation_ID>0</Operation_ID><Threshold>1</Threshold><Fault_Code>101</Fault_Code><Increment/></row>
<row><Class>pumps</Class><Name>trip</Name><Category>domestic</Category><Value>0</Value><Slave_ID>1</Slave_ID><Function_ID>2</Function_ID><Start_Address>12</Start_Address><Data_Length>1</Data_Length><Operation_ID>0</Operation_ID><Threshold>1</Threshold><Fault_Code>101</Fault_Code><Increment/></row>
<row><Class>pumps</Class><Name>trip</Name><Category>jockey</Category><Value>0</Value><Slave_ID>1</Slave_ID><Function_ID>2</Function_ID><Start_Address>4</Start_Address><Data_Length>1</Data_Length><Operation_ID>0</Operation_ID><Threshold>1</Threshold><Fault_Code>101</Fault_Code><Increment/></row>
<row><Class>pumps</Class><Name>trip</Name><Category>main</Category><Value>0</Value><Slave_ID>1</Slave_ID><Function_ID>2</Function_ID><Start_Address>0</Start_Address><Data_Length>1</Data_Length><Operation_ID>0</Operation_ID><Threshold>1</Threshold><Fault_Code>101</Fault_Code><Increment/></row>
<row><Class>pumps</Class><Name>voltage</Name><Category>type</Category><Value>1</Value><Slave_ID/><Function_ID/><Start_Address/><Data_Length/><Operation_ID/><Threshold/><Fault_Code/><Increment/></row>
<row><Class>pumps</Class><Name>voltage</Name><Category>unit</Category><Value>1</Value><Slave_ID/><Function_ID/><Start_Address/><Data_Length/><Operation_ID/><Threshold/><Fault_Code/><Increment/></row>
<row><Class>pumps</Class><Name>voltage</Name><Category>booster</Category><Value>240</Value><Slave_ID/><Function_ID/><Start_Address/><Data_Length/><Operation_ID>1</Operation_ID><Threshold>200</Threshold><Fault_Code>302</Fault_Code><Increment/></row>
<row><Class>pumps</Class><Name>voltage</Name><Category>booster</Category><Value>240</Value><Slave_ID/><Function_ID/><Start_Address/><Data_Length/><Operation_ID>2</Operation_ID><Threshold>270</Threshold><Fault_Code>301</Fault_Code><Increment>-2</Increment></row>
<row><Class>pumps</Class><Name>voltage</Name><Category>diesel</Category><Value>240</Value><Slave_ID/><Function_ID/><Start_Address/><Data_Length/><Operation_ID>2</Operation_ID><Threshold>270</Threshold><Fault_Code>301</Fault_Code><Increment>-0.5</Increment></row>
<row><Class>pumps</Class><Name>voltage</Name><Category>diesel</Category><Value>240</Value><Slave_ID/><Function_ID/><Start_Address/><Data_Length/><Operation_ID>1</Operation_ID><Threshold>200</Threshold><Fault_Code>302</Fault_Code><Increment/></row>
<row><Class>pumps</Class><Name>voltage</Name><Category>domestic</Category><Value>240</Value><Slave_ID/><Function_ID/><Start_Address/><Data_Length/><Operation_ID>1</Operation_ID><Threshold>200</Threshold><Fault_Code>302</Fault_Code><Increment/></row>
<row><Class>pumps</Class><Name>voltage</Name><Category>domestic</Category><Value>240</Value><Slave_ID/><Function_ID/><Start_Address/><Data_Length/><Operation_ID>2</Operation_ID><Threshold>270</Threshold><Fault_Code>301</Fault_Code><Increment/></row>
<row><Class>pumps</Class><Name>voltage</Name><Category>jockey</Category><Value>240</Value><Slave_ID/><Function_ID/><Start_Address/><Data_Length/><Operation_ID>2</Operation_ID><Threshold>270</Threshold><Fault_Code>301</Fault_Code><Increment/></row>
<row><Class>pumps</Class><Name>voltage</Name><Category>jockey</Category><Value>240</Value><Slave_ID/><Function_ID/><Start_Address/><Data_Length/><Operation_ID>1</Operation_ID><Threshold>200</Threshold><Fault_Code>302</Fault_Code><Increment/></row>
<row><Class>pumps</Class><Name>voltage</Name><Category>main</Category><Value>240</Value><Slave_ID/><Function_ID/><Start_Address/><Data_Length/><Operation_ID>2</Operation_ID><Threshold>270</Threshold><Fault_Code>301</Fault_Code><Increment/></row>
<row><Class>pumps</Class><Name>voltage</Name><Category>main</Category><Value>240</Value><Slave_ID/><Function_ID/><Start_Address/><Data_Length/><Operation_ID>1</Operation_ID><Threshold>200</Threshold><Fault_Code>302</Fault_Code><Increment/></row>
<row><Class>repeater panel</Class><Name>status</Name><Category>type</Category><Value>0</Value><Slave_ID/><Function_ID/><Start_Address/><Data_Length/><Operation_ID/><Threshold/><Fault_Code/><Increment/></row>
<row><Class>repeater panel</Class><Name>status</Name><Category>First Floor</Category><Value>0</Value><Slave_ID/><Function_ID/><Start_Address/><Data_Length/><Operation_ID>0</Operation_ID><Threshold>1</Threshold><Fault_Code>101</Fault_Code><Increment/></row>
<row><Class>repeater panel</Class><Name>status</Name><Category>Fourth Floor</Category><Value>0</Value><Slave_ID/><Function_ID/><Start_Address/><Data_Length/><Operation_ID>0</Operation_ID><Threshold>1</Threshold><Fault_Code>101</Fault_Code><Increment/></row>
<row><Class>repeater panel</Class><Name>status</Name><Category>Ground Floor</Category><Value>0</Value><Slave_ID/><Function_ID/><Start_Address/><Data_Length/><Operation_ID>0</Operation_ID><Threshold>1</Threshold><Fault_Code>101</Fault_Code><Increment/></row>
<row><Class>repeater panel</Class><Name>status</Name><Category>Second Floor</Category><Value>0</Value><Slave_ID/><Function_ID/><Start_Address/><Data_Length/><Operation_ID>0</Operation_ID><Threshold>1</Threshold><Fault_Code>101</Fault_Code><Increment>1</Increment></row>
<row><Class>repeater panel</Class><Name>status</Name><Category>Third Floor</Category><Value>0</Value><Slave_ID/><Function_ID/><Start_Address/><Data_Length/><Operation_ID>0</Operation_ID><Threshold>1</Threshold><Fault_Code>101</Fault_Code><Increment/></row>
<row><Class>ups battery</Class><Name>voltage</Name><Category>type</Category><Value>1</Value><Slave_ID/><Function_ID/><Start_Address/><Data_Length/><Operation_ID/><Threshold/><Fault_Code/><Increment/></row>
<row><Class>ups battery</Class><Name>voltage</Name><Category>unit</Category><Value>1</Value><Slave_ID/><Function_ID/><Start_Address/><Data_Length/><Operation_ID/><Threshold/><Fault_Code/><Increment/></row>
<row><Class>ups battery</Class><Name>voltage</Name><Category>main</Category><Value>240</Value><Slave_ID/><Function_ID/><Start_Address/><Data_Length/><Operation_ID>1</Operation_ID><Threshold>200</Threshold><Fault_Code>302</Fault_Code><Increment>-0.5</Increment></row>
<row><Class>water tanks</Class><Name>level</Name><Category>type</Category><Value>1</Value><Slave_ID/><Function_ID/><Start_Address/><Data_Length/><Operation_ID/><Threshold/><Fault_Code/><Increment/></row>
<row><Class>water tanks</Class><Name>level</Name><Category>unit</Category><Value>1</Value><Slave_ID/><Function_ID/><Start_Address/><Data_Length/><Operation_ID/><Threshold/><Fault_Code/><Increment/></row>
<row><Class>water tanks</Class><Name>level</Name><Category>Overhead A</Category><Value>3000</Value><Slave_ID/><Function_ID/><Start_Address/><Data_Length/><Operation_ID>4</Operation_ID><Threshold>10</Threshold><Fault_Code>201</Fault_Code><Increment/></row>
<row><Class>water tanks</Class><Name>level</Name><Category>Overhead B</Category><Value>3000</Value><Slave_ID/><Function_ID/><Start_Address/><Data_Length/><Operation_ID>4</Operation_ID><Threshold>10</Threshold><Fault_Code>201</Fault_Code><Increment/></row>
<row><Class>water tanks</Class><Name>level</Name><Category>Underground A</Category><Value>3000</Value><Slave_ID>1</Slave_ID><Function_ID>4</Function_ID><Start_Address>68</Start_Address><Data_Length>1</Data_Length><Operation_ID>4</Operation_ID><Threshold>10</Threshold><Fault_Code>201</Fault_Code><Increment/></row>
<row><Class>water tanks</Class><Name>level</Name><Category>Underground B</Category><Value>3000</Value><Slave_ID/><Function_ID/><Start_Address/><Data_Length/><Operation_ID>4</Operation_ID><Threshold>10</Threshold><Fault_Code>201</Fault_Code><Increment/></row>
</Variables>
)XMLRAW";

static const char SETTINGS_XML_DEFAULT[] = R"XMLRAW(
<Settings>
  <row><general><SSID>Airtel_kaus_0496</SSID><Password>air68987</Password><Class_Pool_Size>32</Class_Pool_Size><Var_Pool_Size>128</Var_Pool_Size><Alert_Cooldown(mins)>2</Alert_Cooldown(mins)></general></row>
  <row><mqtt><Host>mqtt-server.ddns.net</Host><Port>1883</Port><Data_Topic>ACMS_Rey/01/</Data_Topic><Alert_Topic>ACMS_Rey/02/</Alert_Topic><Mqtt_Username></Mqtt_Username><Mqtt_Password></Mqtt_Password></mqtt></row>
  <row><json_includes><Metadata>true</Metadata><Constraints>false</Constraints><Type_Unit>true</Type_Unit></json_includes></row>
</Settings>
)XMLRAW";
