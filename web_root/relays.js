function callBackend(relais, enabled, success) {
	const data = {"enabled": enabled};
	$.ajax({
	    type: "POST",
	    url: `/api/relays/${relais}`,
	    data: JSON.stringify(data),
		contentType: 'application/json',
	    success: success,
	    dataType: "json"
	});
}



$(() => {
	$('.relais-button').on('click', (e) => {
		const t = e.target;
		const data = {"action": t.dataset['action']};
		const relais = t.dataset['relais'];
		switch (data["action"]) {
			case "keyPress":
			    callBackend(relais, true, () => {
					setTimeout(() => { callBackend(relais, false, () => {}); }, +t.dataset['duration'] * 1000);
				});
				break;
			case "switch":
			    callBackend(relais, +t.dataset['state'] === 1, () => {});
				break;
			default:
			    console.log(`Unknown action: ${data["action"]}`)
		}
	});
});