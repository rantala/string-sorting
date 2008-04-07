<stylesheet version="1.0" xmlns="http://www.w3.org/1999/XSL/Transform">
	<output method="text" omit-xml-declaration="yes" indent="no"/>

	<strip-space elements="*"/>

	<template name="newline"><text>
</text></template>

	<template match="/profile/binary/count">
		<text>&lt;event name="</text>
		<value-of select="/profile/setup/eventsetup/@eventname"/>
		<text>" mask="</text>
		<value-of select="/profile/setup/eventsetup/@unitmask"/>
		<text>" value="</text>
		<value-of select=". * /profile/setup/eventsetup/@setupcount"/>
		<text>"/&gt;</text>
		<call-template name="newline"/>
	</template>

	<!-- override implicit behaviour -->
	<template match="text()|@*"/>

</stylesheet>
