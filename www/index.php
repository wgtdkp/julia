<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.01//EN" "http://www.w3.org/TR/html4/loose.dtd">
<html>
    <head>
        <title>julia homepage</title>
        <!--<link type="text/css" href="./css/home.css" rel="stylesheet" />-->
    </head>
    <body>
        <div class="header">
            <h1><span style="font-style: italic;">julia</span></h1>
        </div>

        <div class="menu">
            <div class="menu-item">
                <a href="./index.php">home</a>
            </div>
            <div class="menu-item">
                <a href="./index.php?menuitem=document">document</a>
            </div>
            <div class="menu-item">
                <a href="./index.php?menuitem=contribution">contribution</a>
            </div>
            <div class="menu-item">
                <a href="https://www.github.com/wgtdkp/julia">Github</a>
            </div>
        </div>

        <?php
            if (!isset($_GET['menuitem'])) {
                include "home.php";
            } else if ($_GET['menuitem'] == 'document') {
                include "document.php";
            } else if ($_GET['menuitem'] == 'contribution') {
                include "contribution.php";
            } else {
                include "home.php";
            }
        ?>

        <div class="footer">
            <p>powered by julia</p>
        </div>
    </body>
</html>