<?xml version="1.0" encoding="utf-8"?>
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
                version="1.0">

  <xsl:output method="text" indent="no"/>

  <xsl:template match="/AppInfo">
    <xsl:value-of select="About/Version"/>
  </xsl:template>
</xsl:stylesheet>
