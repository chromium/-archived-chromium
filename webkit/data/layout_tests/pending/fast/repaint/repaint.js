function runRepaintTest()
{
    if (window.layoutTestController) {
        layoutTestController.waitUntilDone();
        document.body.offsetTop;
        layoutTestController.display();
        repaintTest();
    } else {
        setTimeout(repaintTest, 0);
    }
}
