<?xml version="1.0" encoding="UTF-8"?>
<simconf>
  <!-- Simulation definition -->
  <simulation>
    <!-- Configs -->
    <title>WSN Project Setup</title>
    <randomseed>123456</randomseed>
    <motedelay_us>1000000</motedelay_us>
    <radiomedium>
      org.contikios.cooja.radiomediums.UDGM
      <transmitting_range>50.0</transmitting_range>
      <interference_range>100.0</interference_range>
      <success_ratio_tx>1.0</success_ratio_tx>
      <success_ratio_rx>0.0</success_ratio_rx>
    </radiomedium>
    <events>
      <logoutput>40000</logoutput>
    </events>

    <!-- Definition of mote -->
    <motetype>
      org.contikios.cooja.mspmote.SkyMoteType
      <identifier>mtype634</identifier>
      <description>Custom Sky Motetype</description>
      <source EXPORT="discard">[CONTIKI_DIR]/shared_files/WSN_project/main.c</source>
      <commands EXPORT="discard">make -j$(CPUS) main.sky TARGET=sky</commands>
      <firmware EXPORT="copy">[CONTIKI_DIR]/shared_files/WSN_project/main.sky</firmware>
      <moteinterface>org.contikios.cooja.interfaces.Position</moteinterface>
      <moteinterface>org.contikios.cooja.interfaces.RimeAddress</moteinterface>
      <moteinterface>org.contikios.cooja.interfaces.IPAddress</moteinterface>
      <moteinterface>org.contikios.cooja.interfaces.Mote2MoteRelations</moteinterface>
      <moteinterface>org.contikios.cooja.interfaces.MoteAttributes</moteinterface>
      <moteinterface>org.contikios.cooja.mspmote.interfaces.MspClock</moteinterface>
      <moteinterface>org.contikios.cooja.mspmote.interfaces.MspMoteID</moteinterface>
      <moteinterface>org.contikios.cooja.mspmote.interfaces.SkyCoffeeFilesystem</moteinterface>
      <moteinterface>org.contikios.cooja.mspmote.interfaces.Msp802154Radio</moteinterface>
      <moteinterface>org.contikios.cooja.mspmote.interfaces.MspSerial</moteinterface>
      <moteinterface>org.contikios.cooja.mspmote.interfaces.MspDebugOutput</moteinterface>
    </motetype>

    <!-- Declaration of mote A, B and C -->
    <!-- A -->
    <mote>
      <interface_config>
        org.contikios.cooja.interfaces.Position
        <x>0.0</x>
        <y>0.0</y>
        <z>0.0</z>
      </interface_config>
      <interface_config>
        org.contikios.cooja.mspmote.interfaces.MspMoteID
        <id>1</id> 
      </interface_config>
      <interface_config>
        org.contikios.cooja.mspmote.interfaces.Msp802154Radio
        <bitrate>250.0</bitrate>
      </interface_config>
      <motetype_identifier>mtype634</motetype_identifier>
    </mote>
    <!-- B -->
    <mote>
      <interface_config>
        org.contikios.cooja.interfaces.Position
        <x>0.0</x>
        <y>0.0</y>
        <z>0.0</z>
      </interface_config>
      <interface_config>
        org.contikios.cooja.mspmote.interfaces.MspMoteID
        <id>2</id>
      </interface_config>
      <interface_config>
        org.contikios.cooja.mspmote.interfaces.Msp802154Radio
        <bitrate>250.0</bitrate>
      </interface_config>
      <motetype_identifier>mtype634</motetype_identifier>
    </mote>
    <!-- C -->
    <mote>
      <interface_config>
        org.contikios.cooja.interfaces.Position
        <x>40.0</x>
        <y>0.0</y>
        <z>0.0</z>
      </interface_config>
      <interface_config>
        org.contikios.cooja.mspmote.interfaces.MspMoteID
        <id>3</id>
      </interface_config>
      <interface_config>
        org.contikios.cooja.mspmote.interfaces.Msp802154Radio
        <bitrate>250.0</bitrate>
      </interface_config>
      <motetype_identifier>mtype634</motetype_identifier>
    </mote>
  </simulation>

  <!-- Plugins for info about system (I Guess) -->
  <plugin>
    org.contikios.cooja.plugins.SimControl
    <width>280</width>
    <z>4</z>
    <height>160</height>
    <location_x>0</location_x>
    <location_y>0</location_y>
  </plugin>
  <plugin>
    org.contikios.cooja.plugins.Visualizer
    <plugin_config>
      <moterelations>true</moterelations>
      <skin>org.contikios.cooja.plugins.skins.IDVisualizerSkin</skin>
      <skin>org.contikios.cooja.plugins.skins.UDGMVisualizerSkin</skin>
      <viewport>6.0 0.0 0.0 6.0 0.0 0.0</viewport>
    </plugin_config>
    <width>400</width>
    <z>1</z>
    <height>400</height>
    <location_x>1</location_x>
    <location_y>1</location_y>
  </plugin>
  <plugin>
    org.contikios.cooja.plugins.LogListener
    <plugin_config>
      <filter />
      <formatted_time />
      <coloring />
    </plugin_config>
    <width>1179</width>
    <z>0</z>
    <height>704</height>
    <location_x>679</location_x>
    <location_y>0</location_y>
  </plugin>
  <plugin>
    org.contikios.cooja.plugins.TimeLine
    <plugin_config>
      <mote>0</mote>
      <mote>1</mote>
      <mote>2</mote>
      <showRadioRXTX />
      <showRadioHW />
      <showLEDs />
      <zoomfactor>1.7067792216977151</zoomfactor>
    </plugin_config>
    <width>1858</width>
    <z>3</z>
    <height>166</height>
    <location_x>9</location_x>
    <location_y>723</location_y>
  </plugin>
  <plugin>
    org.contikios.cooja.plugins.RadioLogger
    <plugin_config>
      <split>150</split>
      <formatted_time />
      <showdups>false</showdups>
      <hidenodests>false</hidenodests>
    </plugin_config>
    <width>500</width>
    <z>2</z>
    <height>300</height>
    <location_x>109</location_x>
    <location_y>408</location_y>
  </plugin>
</simconf>
