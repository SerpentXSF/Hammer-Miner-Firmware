self.addEventListener('message', function (e) {
    const {interval} = e.data;
    setInterval(() => {
        postMessage({});
    }, interval);
});
